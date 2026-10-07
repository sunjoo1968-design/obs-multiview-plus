#pragma once

#include "tally-obs.hpp"
#include "gpu-capture.hpp"
#include <obs-module.h>
#include <QTimer>
#include <QString>
#include <functional>
#include <memory>

namespace mvtest {
// Optional test of the real Source Switcher module, not a fake enum callback.
// Run on the Qt GUI thread in the isolated smoke-test OBS process only.
inline void runSwitcherFixture(const QString &directory, std::function<void(QString, bool)> record,
                               std::function<void()> done)
{
    if (!obs_get_module("source-switcher")) {
        done();
        return;
    }
    struct State {
        obs_source_t *camera[2]{};
        obs_source_t *switcher{};
        obs_scene_t *root{}, *mapping{}, *unrelated{};
        obs_sceneitem_t *edge{};
        bool showing{};
        QString directory;
        std::function<void(QString, bool)> record;
        std::function<void()> done;
        ~State()
        {
            if (showing) obs_source_dec_showing(switcher);
            if (root) obs_scene_release(root);
            if (mapping) obs_scene_release(mapping);
            if (unrelated) obs_scene_release(unrelated);
            if (switcher) { obs_source_remove(switcher); obs_source_release(switcher); }
            for (auto *source : camera) {
                if (source) { obs_source_remove(source); obs_source_release(source); }
            }
        }
        bool on(int index) const
        {
            return mv::sourceOnRoot(obs_scene_get_source(root), camera[index]);
        }
        void check(const char *key, bool pass) { record(QString::fromLatin1(key), pass); }
        void capture(const char *phase)
        {
            const QString file = directory + "/switcher-" + QString::fromLatin1(phase) + ".png";
            record("switcher_gpu_" + QString::fromLatin1(phase), captureGpu(switcher, false, file));
        }
    };
    auto s = std::make_shared<State>();
    s->record = std::move(record);
    s->done = std::move(done);
    s->directory = directory;
    for (int i = 0; i < 2; ++i) {
        auto *settings = obs_data_create();
        obs_data_set_int(settings, "width", 320);
        obs_data_set_int(settings, "height", 180);
        obs_data_set_int(settings, "color", i ? 0xff00ff00 : 0xff0000ff);
        s->camera[i] = obs_source_create("color_source_v3", i ? "MV Switcher Fixture B" : "MV Switcher Fixture A", settings, nullptr);
        obs_data_release(settings);
    }
    auto *settings = obs_data_create_from_json(
        "{\"sources\":[{\"value\":\"MV Switcher Fixture A\"},{\"value\":\"MV Switcher Fixture B\"}],"
        "\"transition\":\"cut_transition\",\"transition_duration\":700,\"show_transition\":\"\",\"hide_transition\":\"\","
        "\"time_switch\":false,\"media_state_switch\":false,\"current_source_file\":false,\"transition_resize\":false}");
    s->switcher = obs_source_create("source_switcher", "MV Switcher Fixture Output", settings, nullptr);
    obs_data_release(settings);
    s->root = obs_scene_create_private("MV Switcher Fixture Root");
    s->mapping = obs_scene_create_private("MV Switcher Fixture Mapping");
    s->unrelated = obs_scene_create_private("MV Switcher Fixture Unrelated");
    const bool ready = s->camera[0] && s->camera[1] && s->switcher && s->root && s->mapping && s->unrelated;
    s->check("switcher_fixture_created", ready);
    if (!ready) { auto finish = s->done; s.reset(); finish(); return; }
    s->edge = obs_scene_add(s->root, s->switcher);
    obs_scene_add(s->mapping, s->camera[0]);
    obs_source_inc_showing(s->switcher);
    s->showing = true;
    // Same media seek endpoint used by Camera MIX Controller's ME1.
    obs_source_media_set_time(s->switcher, 0);
    QTimer::singleShot(250, [s] {
        s->capture("initial");
        s->check("switcher_current_only", s->on(0) && !s->on(1));
        s->check("switcher_scene_to_raw_mapping", mv::sourceOnRoot(obs_scene_get_source(s->root), obs_scene_get_source(s->mapping)));
        s->check("switcher_off_air_excluded", !mv::sourceOnRoot(obs_scene_get_source(s->unrelated), s->camera[0]));
        obs_sceneitem_set_visible(s->edge, false);
        s->check("switcher_hidden_parent_excluded", !s->on(0) && !s->on(1));
        obs_sceneitem_set_visible(s->edge, true);
        obs_source_media_set_time(s->switcher, 1000);
        QTimer::singleShot(200, [s] {
            s->capture("cut");
            s->check("switcher_cut_current_only", !s->on(0) && s->on(1));
            auto *data = obs_source_get_settings(s->switcher);
            obs_data_set_string(data, "transition", "fade_transition");
            obs_source_update(s->switcher, data);
            obs_data_release(data);
            // Source updates may run on the OBS video thread; allow frames to commit.
            QTimer::singleShot(200, [s] {
                s->capture("before-mix");
                s->check("switcher_before_mix", !s->on(0) && s->on(1));
                obs_source_media_set_time(s->switcher, 0);
                QTimer::singleShot(250, [s] {
                    s->capture("during-mix");
                    bool progressing = false;
                    obs_source_enum_active_sources(s->switcher, [](obs_source_t *, obs_source_t *child, void *data) {
                        if (obs_source_get_type(child) == OBS_SOURCE_TYPE_TRANSITION) {
                            const float progress = obs_transition_get_time(child);
                            if (progress > 0.0f && progress < 1.0f) *static_cast<bool *>(data) = true;
                        }
                    }, &progressing);
                    s->check("switcher_mix_progress_interior", progressing);
                    const QImage mixed(s->directory + "/switcher-during-mix.png");
                    const QColor center = mixed.isNull() ? QColor() : mixed.pixelColor(mixed.width() / 2, mixed.height() / 2);
                    s->check("switcher_mix_pixels_both_colors", center.isValid() && center.red() > 20 && center.green() > 20 && center.blue() < 20);
                    s->check("switcher_mix_both_contributors", s->on(0) && s->on(1));
                    QTimer::singleShot(800, [s]() mutable {
                        s->capture("after-mix");
                        s->check("switcher_mix_completed_old_off", s->on(0) && !s->on(1));
                        auto finish = s->done;
                        s.reset();
                        finish();
                    });
                });
            });
        });
    });
}
} // namespace mvtest
