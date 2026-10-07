#pragma once

#include "tally-graph.hpp"
#include <obs.h>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>

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
    };
    std::unordered_map<obs_source_t *, std::vector<Edge>> snapshots;
    auto children = [&](obs_source_t *source) -> const std::vector<Edge> & {
        auto inserted = snapshots.try_emplace(source);
        auto &edges = inserted.first->second;
        if (!inserted.second || !source)
            return edges;
        auto append = [&](obs_source_t *child, bool visible) {
            SourceRef reference(obs_source_get_ref(child), &obs_source_release);
            if (reference)
                edges.push_back({std::move(reference), visible});
        };
        if (auto *scene = obs_group_or_scene_from_source(source)) {
            obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *item, void *data) {
                auto &snapshot = *static_cast<decltype(append) *>(data);
                snapshot(obs_sceneitem_get_source(item), obs_sceneitem_visible(item));
                return true;
            }, &append);
        } else {
            // Includes private Fade A/B and Source Switcher's active output.
            // All-source enumeration would incorrectly tally inactive cameras.
            const char *id = obs_source_get_unversioned_id(source);
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
        }
        return edges;
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
    return graphSourceOnRoot(root, target, explicitTallySource, enumerate, container, videoLeaf);
}
} // namespace mv
