#pragma once

#include "tally-graph.hpp"
#include <obs.h>

namespace mv {
// Borrowed source pointers. Callers hold references for the duration of this
// call. An explicitly configured but missing UUID must be rejected by caller.
inline bool sourceOnRoot(obs_source_t *root, obs_source_t *target,
                         obs_source_t *explicitTallySource = nullptr)
{
    auto enumerate = [](obs_source_t *source, auto visit) {
        if (auto *scene = obs_group_or_scene_from_source(source)) {
            obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *item, void *data) {
                auto &visitor = *static_cast<decltype(visit) *>(data);
                visitor(obs_sceneitem_get_source(item), obs_sceneitem_visible(item));
                return true;
            }, &visit);
        }
    };
    auto container = [](obs_source_t *source) {
        return obs_source_is_scene(source) || obs_source_is_group(source);
    };
    auto videoLeaf = [&](obs_source_t *source) {
        return !container(source) && (obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO) != 0;
    };
    return graphSourceOnRoot(root, target, explicitTallySource, enumerate, container, videoLeaf);
}
} // namespace mv
