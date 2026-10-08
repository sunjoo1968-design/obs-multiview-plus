#pragma once

#include "tally-graph.hpp"
#include <obs.h>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

namespace mv {
// Borrowed source pointers. Callers hold references for the duration of this
// call. An explicitly configured but missing UUID must be rejected by caller.
inline bool sourceOnRoot(obs_source_t *root, obs_source_t *target,
                         obs_source_t *explicitTallySource = nullptr)
{
    // Retain every discovered child until the entire query ends, including the
    // leaf returned by fallback matching. Enumerators may hold scene/transition
    // locks: callbacks only snapshot edges, never recurse into another source.
    using SourceRef = std::unique_ptr<obs_source_t, decltype(&obs_source_release)>;
    struct Edge {
        SourceRef source;
        bool visible;
        int64_t itemId;
    };
    std::unordered_map<obs_source_t *, std::vector<Edge>> snapshots;
    auto children = [&](obs_source_t *source) -> const std::vector<Edge> & {
        auto inserted = snapshots.try_emplace(source);
        auto &edges = inserted.first->second;
        if (!inserted.second || !source || snapshots.size() > 8192)
            return edges;
        auto append = [&](obs_source_t *child, bool visible, int64_t itemId = -1) {
            SourceRef reference(obs_source_get_ref(child), &obs_source_release);
            if (reference)
                edges.push_back({std::move(reference), visible, itemId});
        };
        if (auto *scene = obs_group_or_scene_from_source(source)) {
            obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *item, void *data) {
                auto &snapshot = *static_cast<decltype(append) *>(data);
                snapshot(obs_sceneitem_get_source(item), obs_sceneitem_visible(item), obs_sceneitem_get_id(item));
                return true;
            }, &append);
        } else {
            // Includes private Fade A/B and Source Switcher's active output.
            // All-source enumeration would incorrectly tally inactive cameras.
            const char *id = obs_source_get_unversioned_id(source);
            const bool cut = obs_source_get_type(source) == OBS_SOURCE_TYPE_TRANSITION && id && std::strcmp(id, "cut_transition") == 0;
            const bool fade = obs_source_get_type(source) == OBS_SOURCE_TYPE_TRANSITION &&
                              id && std::strcmp(id, "fade_transition") == 0;
            float fadeTime = 0.5f;
            auto appendActive = [&](obs_source_t *child) {
                // Native transition enumeration holds its state lock across
                // both callbacks. Read the video time here so the A/B snapshot
                // and timing parameters cannot belong to different starts.
                if (fade && edges.empty())
                    fadeTime = obs_transition_get_time(source);
                append(child, true);
            };
            obs_source_enum_active_sources(source,
                [](obs_source_t *, obs_source_t *child, void *data) {
                    auto &snapshot = *static_cast<decltype(appendActive) *>(data);
                    snapshot(child);
                }, &appendActive);
            // Fade may retain A/B for an audio tail after the video has already
            // reached B. Tally tracks video contribution, not audio lifetime.
            // One child is an idle output; never suppress it based on old time.
            // Other transition types may use a different curve (e.g. Stinger).
            if (fade && edges.size() == 2) {
                edges[0].visible = fadeTime < 1.0f;
                edges[1].visible = fadeTime > 0.0f;
            }
            if (cut) {
                SourceRef target(obs_transition_get_active_source(source), &obs_source_release);
                if (target) for (auto &edge : edges) edge.visible = edge.source.get() == target.get();
            }

        }
        return edges;
    };
    // Match frozen identity while traversing the COPIED item visibility.
    // Only outer Studio roots and UUID-marked Hybrid snapshots may alias.
    std::unordered_map<obs_source_t *, SourceRef> aliases;
    std::unordered_set<obs_source_t *> rootVisited;
    auto roots = [&](auto &&self, obs_source_t *node) -> void {
        if (!node || !rootVisited.insert(node).second || rootVisited.size() > 8192) return;
        if (obs_source_is_scene(node)) {
            if (obs_obj_is_private(node)) {
                SourceRef original(obs_get_source_by_name(obs_source_get_name(node)), &obs_source_release);
                if (original && obs_source_is_scene(original.get()) && !obs_obj_is_private(original.get()))
                    aliases.insert_or_assign(node, std::move(original));
            }
            return;
        }
        if (obs_source_get_type(node) == OBS_SOURCE_TYPE_TRANSITION)
            for (const auto &edge : children(node)) if (edge.visible) self(self, edge.source.get());
    };
    roots(roots, root);
    std::unordered_set<obs_source_t *> visited, paired;
    auto pair = [&](auto &&self, obs_source_t *copy, obs_source_t *original, size_t depth) -> void {
        if (!copy || !original || copy == original || !paired.insert(copy).second) return;
        if (depth > 128 || paired.size() > 8192) return;
        const char *copyId = obs_source_get_unversioned_id(copy);
        const char *originalId = obs_source_get_unversioned_id(original);
        if (!copyId || !originalId || strcmp(copyId, originalId)) return;
        if (obs_obj_is_private(copy) && !obs_obj_is_private(original))
            aliases.insert_or_assign(copy, SourceRef(obs_source_get_ref(original), &obs_source_release));
        if (!obs_group_or_scene_from_source(copy) || !obs_group_or_scene_from_source(original)) return;
        std::unordered_map<int64_t, obs_source_t *> originals;
        std::unordered_map<std::string, obs_source_t *> identities;
        auto identity = [](obs_source_t *source) {
            const char *id = obs_source_get_unversioned_id(source);
            return std::string(obs_source_get_name(source)) + "\x1f" + (id ? id : "");
        };
        for (const auto &edge : children(original)) {
            originals.emplace(edge.itemId, edge.source.get());
            auto entry = identities.emplace(identity(edge.source.get()), edge.source.get());
            if (!entry.second && entry.first->second != edge.source.get()) entry.first->second = nullptr;
        }
        for (const auto &edge : children(copy)) {
            auto found = originals.find(edge.itemId);
            obs_source_t *source = found == originals.end() ? nullptr : found->second;
            const auto key = identity(edge.source.get());
            // OBS creates new scene-item IDs for private duplicates. Fallback
            // is limited to a unique name/type INSIDE the paired original.
            if (!source || identity(source) != key) {
                auto match = identities.find(key);
                    source = match == identities.end() ? nullptr : match->second;
                }
                if (!source) continue;
                self(self, edge.source.get(), source, depth + 1);
            }
        };
        auto walk = [&](auto &&self, obs_source_t *node, size_t depth) -> void {
            if (!node || !visited.insert(node).second) return;
            if (depth > 128 || visited.size() > 8192) return;
            const char *id = obs_source_get_unversioned_id(node);
            if (id && !strcmp(id, "camera_mix_hybrid_output") && obs_obj_is_private(node)) {
                for (const auto &edge : children(node)) if (edge.visible) {
                    std::unique_ptr<obs_data_t, decltype(&obs_data_release)> meta(obs_source_get_private_settings(edge.source.get()), obs_data_release);
                    const char *uuid = obs_data_get_string(meta.get(), "camera_mix_hybrid_original_uuid");
                    if (!uuid || !*uuid || strlen(uuid) > 64) continue;
                    SourceRef original(obs_get_source_by_uuid(uuid), obs_source_release);
                    if (original) pair(pair, edge.source.get(), original.get(), 0);
                }
            }
            for (const auto &edge : children(node)) if (edge.visible) self(self, edge.source.get(), depth + 1);
        };
        walk(walk, root, 0);

    auto match = [&](obs_source_t *node, obs_source_t *wanted) {
        if (node == wanted) return true;
        auto found = aliases.find(node);
        return found != aliases.end() && found->second.get() == wanted;
    };
    auto enumerate = [&](obs_source_t *source, auto visit) {
        for (const auto &edge : children(source))
            visit(edge.source.get(), edge.visible);
    };
    auto container = [](obs_source_t *source) {
        return obs_source_is_scene(source) || obs_source_is_group(source);
    };
    auto videoLeaf = [&](obs_source_t *source) {
        return !container(source) && obs_source_get_type(source) != OBS_SOURCE_TYPE_TRANSITION &&
               children(source).empty() &&
               (obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO) != 0;
    };
    return graphSourceOnRoot(root, target, explicitTallySource, enumerate, container, videoLeaf, match);
}
} // namespace mv
