#pragma once

#include "tally-obs.hpp"
#include "gpu-capture.hpp"
#include <QTimer>
#include <functional>
#include <memory>
#include <vector>

namespace mvtest {
// Isolated test module only: no frontend output, collection or user sources touched.
class ControllerFixture : public std::enable_shared_from_this<ControllerFixture> {
    std::function<void(const QString &, bool)> record;
    std::function<void()> complete;
    QString directory;
    std::vector<obs_scene_t *> scenes;
    obs_source_t *camera[2]{};
    obs_source_t *fade{};
    obs_scene_t *views[2]{};
    obs_scene_t *root{};
    obs_scene_t *output{};
    obs_scene_t *alias{};
    obs_sceneitem_t *parent{};
    obs_sceneitem_t *runtime{};
    bool showing = false;

    obs_scene_t *scene(const char *name)
    {
        auto *value = obs_scene_create_private(name);
        if (value) scenes.push_back(value);
        return value;
    }
    bool on(obs_source_t *target, obs_source_t *mapping = nullptr)
    {
        return mv::sourceOnRoot(obs_scene_get_source(root), target, mapping);
    }
    void done()
    {
        if (showing) obs_source_dec_showing(obs_scene_get_source(root));
        showing = false;
        if (fade) obs_transition_clear(fade);
        // Detach shared scene/group items before releasing private containers.
        // Their OBS destruction is deferred and may otherwise retain one another
        // until shutdown when groups are reused by multiple private views.
        for (auto *value : scenes)
            obs_scene_enum_items(value, [](obs_scene_t *, obs_sceneitem_t *item, void *) {
                obs_sceneitem_remove(item);
                return true;
            }, nullptr);
        // Parent scenes were allocated last, release them first.
        for (auto it = scenes.rbegin(); it != scenes.rend(); ++it) obs_scene_release(*it);
        scenes.clear();
        if (fade) obs_source_release(fade);
        fade = nullptr;
        for (auto *&source : camera) { if (source) obs_source_release(source); source = nullptr; }
        obs_wait_for_destroy_queue();
        complete();
    }
public:
    ControllerFixture(std::function<void(const QString &, bool)> r,
                      std::function<void()> c, QString path)
        : record(std::move(r)), complete(std::move(c)), directory(std::move(path)) {}
    void start()
    {
        auto *settings = obs_data_create();
        obs_data_set_int(settings, "width", 1920);
        obs_data_set_int(settings, "height", 1080);
        obs_data_set_int(settings, "color", 0xff0000ff);
        camera[0] = obs_source_create_private("color_source_v3", "controller camera A", settings);
        obs_data_set_int(settings, "color", 0xff00ff00);
        camera[1] = obs_source_create_private("color_source_v3", "controller camera B", settings);
        obs_data_release(settings);
        auto *manager = scene("controller hidden inputs");
        views[0] = scene("controller private view A");
        views[1] = scene("controller private view B");
        alias = scene("controller ME1 alias");
        output = scene("controller runtime output");
        root = scene("controller simulated PGM");
        fade = obs_source_create_private("fade_transition", "controller private MIX", nullptr);
        const bool created = camera[0] && camera[1] && manager && views[0] && views[1] && alias && output && root && fade;
        record("controller_fixture_created", created);
        if (!created) { done(); return; }
        for (int i = 0; i < 2; ++i) {
            auto *group = obs_scene_add_group2(manager, i ? "controller group B" : "controller group A", false);
            if (!group) { record("controller_group_created", false); done(); return; }
            obs_scene_add(obs_sceneitem_group_get_scene(group), camera[i]);
            obs_scene_add(views[i], obs_sceneitem_get_source(group));
            // Storage visibility is independent of the visible private view path.
            obs_sceneitem_set_visible(group, i == 0);
        }
        obs_scene_add(alias, camera[1]);
        auto *storage = obs_scene_add(output, obs_scene_get_source(manager));
        obs_sceneitem_set_visible(storage, false);
        obs_transition_set_size(fade, 1920, 1080);
        obs_transition_set(fade, obs_scene_get_source(views[0]));
        runtime = obs_scene_add(output, fade);
        parent = obs_scene_add(root, obs_scene_get_source(output));
        obs_source_inc_showing(obs_scene_get_source(root));
        showing = true;
        record("controller_idle_selected_only", on(camera[0]) && !on(camera[1]));
        obs_sceneitem_set_visible(parent, false);
        record("controller_hidden_parent_suppresses", !on(camera[0]) && !on(camera[1]));
        obs_transition_set(fade, obs_scene_get_source(views[1]));
        record("controller_off_output_selection_not_program", !on(camera[1]));
        obs_sceneitem_set_visible(parent, true);
        record("controller_hidden_storage_excluded", !on(camera[0]) && on(camera[1]));
        record("controller_single_leaf_alias", on(obs_scene_get_source(alias)));
        record("controller_explicit_reference_mapping", on(camera[0], camera[1]));
        obs_source_set_name(camera[1], "controller renamed camera B");
        obs_source_set_name(fade, "controller renamed runtime");
        record("controller_rename_identity", on(camera[1]));
        auto *duplicate = obs_scene_add(root, camera[0]);
        record("controller_multiple_paths_union", on(camera[0]) && on(camera[1]));
        obs_sceneitem_set_visible(duplicate, false);
        record("controller_hidden_duplicate_ignored", !on(camera[0]) && on(camera[1]));
        obs_sceneitem_remove(duplicate);
        // Replace the private runtime object exactly as an engine rebuild does.
        obs_sceneitem_remove(runtime);
        obs_transition_clear(fade);
        obs_source_release(fade);
        fade = obs_source_create_private("fade_transition", "controller rebuilt runtime", nullptr);
        if (!fade) { record("controller_rebuild_created", false); done(); return; }
        obs_transition_set_size(fade, 1920, 1080);
        obs_transition_set(fade, obs_scene_get_source(views[0]));
        runtime = obs_scene_add(output, fade);
        record("controller_rebuild_new_path", on(camera[0]) && !on(camera[1]));
        auto self = shared_from_this();
        QTimer::singleShot(150, [self] {
            captureGpu(obs_scene_get_source(self->root), false, self->directory + "/controller-before.png");
            self->record("controller_auto_mix_started", obs_transition_start(self->fade, OBS_TRANSITION_MODE_AUTO, 1000,
                         obs_scene_get_source(self->views[1])));
            QTimer::singleShot(350, [self] {
                self->record("controller_mid_mix_pixels", captureGpu(obs_scene_get_source(self->root), false,
                             self->directory + "/controller-mid.png"));
                const float time = obs_transition_get_time(self->fade);
                self->record("controller_mid_mix_actual_progress", time > 0.0f && time < 1.0f);
                self->record("controller_mid_mix_both_program", self->on(self->camera[0]) && self->on(self->camera[1]));
                QTimer::singleShot(1000, [self] {
                    self->record("controller_completed_pixels", captureGpu(obs_scene_get_source(self->root), false,
                                 self->directory + "/controller-after.png"));
                    self->record("controller_completed_old_off", !self->on(self->camera[0]) && self->on(self->camera[1]));
                    self->done();
                });
            });
        });
    }
};
inline void runControllerFixture(const QString &directory,
                                 std::function<void(const QString &, bool)> record,
                                 std::function<void()> complete)
{
    std::make_shared<ControllerFixture>(std::move(record), std::move(complete), directory)->start();
}
} // namespace mvtest
