// Test-only module. Never install or distribute with the production plugin.
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/bmem.h>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QScreen>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>
#include <QLabel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSpinBox>
#include <QDockWidget>
#include <QTreeView>
#include <QSet>
#include "tally-obs.hpp"
#include "version.hpp"
#include "gpu-capture.hpp"
#include "controller-fixture.hpp"
#include "switcher-fixture.hpp"

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_name(void) { return "Multiview isolated smoke driver"; }
MODULE_EXPORT const char *obs_module_description(void) { return "TEST ONLY: guarded portable OBS smoke test"; }

namespace {
QString output;
QJsonObject results;
QJsonArray failures;
QPointer<QAction> launchAction;
bool started = false;
bool finished = false;

void checkSceneAnchor(const QString &phase)
{
    if (!obs_get_module("scene-anchor")) return;
    auto *main = static_cast<QWidget *>(obs_frontend_get_main_window());
    auto *dock = main ? main->findChild<QDockWidget *>("scene_anchor_dock") : nullptr;
    auto *tree = dock ? dock->findChild<QTreeView *>() : nullptr;
    QSet<QString> names;
    if (tree && tree->model()) {
        auto walk = [&](auto &&self, const QModelIndex &parent) -> void {
            for (int row = 0; row < tree->model()->rowCount(parent); ++row) {
                auto index = tree->model()->index(row, 0, parent);
                names.insert(index.data().toString());
                self(self, index);
            }
        };
        walk(walk, QModelIndex());
    }
    obs_frontend_source_list scenes{};
    obs_frontend_get_scenes(&scenes);
    bool complete = tree && scenes.sources.num > 0;
    for (size_t i = 0; i < scenes.sources.num; ++i)
        complete = complete && names.contains(QString::fromUtf8(obs_source_get_name(scenes.sources.array[i])));
    results[phase + "_frontend_count"] = int(scenes.sources.num);
    results[phase + "_tree_names"] = int(names.size());
    results[phase + "_scene_anchor_complete"] = complete;
    if (!complete) failures.append(phase + "_scene_anchor_complete");
    obs_frontend_source_list_free(&scenes);
}

void record(const QString &key, bool success)
{
    results[key] = success;
    blog(success ? LOG_INFO : LOG_ERROR, "[mv-smoke] %s: %s", key.toUtf8().constData(), success ? "PASS" : "FAIL");
    if (!success) failures.append(key);
}
QWidget *findWindow(const QString &title)
{
    for (auto *widget : QApplication::topLevelWidgets())
        if ((widget->windowTitle() == title || (qEnvironmentVariableIsSet("MV_TEST_LEGACY_VERSION") &&
             title.startsWith("Sunjoo OBS Link Multiview") && widget->windowTitle() == "Sunjoo OBS Link Multiview")) && widget->isVisible()) return widget;
    return nullptr;
}
void capture(QWidget *widget, const QString &name)
{
    if (!widget) { record(name + "_window", false); return; }
    auto *screen = widget->screen();
    const auto pixels = screen ? screen->grabWindow(widget->winId()) : QPixmap();
    record(name + "_native_png", !pixels.isNull() && pixels.save(output + "/" + name + "-native.png"));
    if (screen) {
        const QPoint origin = widget->mapToGlobal(QPoint(0, 0)) - screen->geometry().topLeft();
        const auto composed = screen->grabWindow(0, origin.x(), origin.y(), widget->width(), widget->height());
        record(name + "_composited_png", !composed.isNull() && composed.save(output + "/" + name + "-composited.png"));
    }
    record(name + "_widget_png", widget->grab().save(output + "/" + name + "-widget.png"));
    results[name + "_width"] = widget->width();
    results[name + "_height"] = widget->height();
}
void finish()
{
    if (finished) return;
    finished = true;
    results["failures"] = failures;
    results["passed"] = failures.isEmpty();
    results["scope"] = "isolated portable instance: rendering captures, settings accept, close/reopen";
    QFile file(output + "/result.json");
    if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(results).toJson());
    blog(LOG_INFO, "[mv-smoke] finished with %lld failures", static_cast<long long>(failures.size()));
    // Resolve any unexpected nested dialog so normal OBS shutdown can complete.
    for (auto *w : QApplication::topLevelWidgets())
        if (auto *dialog = qobject_cast<QDialog *>(w)) dialog->reject();
    QTimer::singleShot(100, qApp, [] {
        auto *main = static_cast<QWidget *>(obs_frontend_get_main_window());
        if (main) main->close();
    });
}
void exerciseTallyGraphs()
{
    obs_source_t *cameras[4]{};
    obs_source_t *cameraScenes[4]{};
    for (int i = 0; i < 4; ++i) {
        cameras[i] = obs_get_source_by_name(QString("MV Smoke Color %1").arg(i + 1).toUtf8().constData());
        cameraScenes[i] = obs_get_source_by_name(QString("MV Smoke Scene %1").arg(i + 1).toUtf8().constData());
    }
    auto *pgm = obs_scene_create("MV Smoke PGM");
    auto *sceneGroupItem = obs_scene_add_group(pgm, "MV Smoke CAM-PGM");
    auto *sceneGroup = obs_sceneitem_group_get_scene(sceneGroupItem);
    auto *raw = obs_scene_create("MV Smoke Lower-PGM");
    auto *rawGroupItem = obs_scene_add_group(raw, "MV Smoke CAM-PGM-B");
    auto *rawGroup = obs_sceneitem_group_get_scene(rawGroupItem);
    obs_sceneitem_t *sceneItems[4]{};
    obs_sceneitem_t *rawItems[4]{};
    for (int i = 0; i < 4; ++i) {
        sceneItems[i] = obs_scene_add(sceneGroup, cameraScenes[i]);
        rawItems[i] = obs_scene_add(rawGroup, cameras[i]);
        obs_sceneitem_set_visible(sceneItems[i], i == 1);
        obs_sceneitem_set_visible(rawItems[i], i == 1);
        record(QString("direct_scene_camera_%1").arg(i + 1), mv::sourceOnRoot(cameraScenes[i], cameras[i]));
    }
    for (int i = 0; i < 4; ++i) {
        record(QString("grouped_scene_%1").arg(i + 1), mv::sourceOnRoot(obs_scene_get_source(pgm), cameraScenes[i]) == (i == 1));
        record(QString("grouped_raw_camera_%1").arg(i + 1), mv::sourceOnRoot(obs_scene_get_source(raw), cameras[i]) == (i == 1));
        record(QString("raw_camera_to_scene_tile_%1").arg(i + 1), mv::sourceOnRoot(obs_scene_get_source(raw), cameraScenes[i]) == (i == 1));
    }
    auto *outer = obs_scene_create("MV Smoke Event Wrapper");
    auto *outerGroupItem = obs_scene_add_group(outer, "MV Smoke Outer Group");
    auto *outerGroup = obs_sceneitem_group_get_scene(outerGroupItem);
    obs_scene_add(outerGroup, obs_scene_get_source(raw));
    record("nested_scene_group_raw_camera", mv::sourceOnRoot(obs_scene_get_source(outer), cameraScenes[1]));
    obs_sceneitem_set_visible(rawGroupItem, false);
    record("hidden_inner_parent_blocks", !mv::sourceOnRoot(obs_scene_get_source(outer), cameraScenes[1]));
    obs_sceneitem_set_visible(rawGroupItem, true);
    obs_sceneitem_set_visible(outerGroupItem, false);
    record("hidden_outer_parent_blocks", !mv::sourceOnRoot(obs_scene_get_source(outer), cameraScenes[1]));
    obs_sceneitem_set_visible(outerGroupItem, true);
    obs_sceneitem_set_visible(rawItems[1], false);
    obs_sceneitem_set_visible(rawItems[2], true);
    record("camera_selection_changed_old_off", !mv::sourceOnRoot(obs_scene_get_source(outer), cameraScenes[1]));
    record("camera_selection_changed_new_on", mv::sourceOnRoot(obs_scene_get_source(outer), cameraScenes[2]));
    // Complex camera scenes require explicit tally binding, avoiding shared-logo matches.
    auto *complex = obs_scene_create("MV Smoke Complex Camera");
    obs_scene_add(complex, cameras[0]);
    obs_scene_add(complex, cameras[2]);
    record("complex_scene_no_automatic_false_positive", !mv::sourceOnRoot(obs_scene_get_source(outer), obs_scene_get_source(complex)));
    record("explicit_camera_binding", mv::sourceOnRoot(obs_scene_get_source(outer), obs_scene_get_source(complex), cameras[2]));
    obs_frontend_set_preview_program_mode(false);
    obs_frontend_set_current_scene(obs_scene_get_source(pgm));
    obs_frontend_set_preview_program_mode(true);
    obs_frontend_set_current_preview_scene(obs_scene_get_source(outer));
    obs_scene_release(complex);
    obs_scene_release(outer);
    obs_scene_release(raw);
    obs_scene_release(pgm);
    for (int i = 0; i < 4; ++i) { obs_source_release(cameras[i]); obs_source_release(cameraScenes[i]); }
}
void createScenes()
{
    const uint32_t colors[] = {0xff4040d0, 0xff50b040, 0xffd07030, 0xff30b0d0};
    for (int i = 0; i < 4; ++i) {
        const auto sourceName = QString("MV Smoke Color %1").arg(i + 1).toUtf8();
        auto *settings = obs_data_create();
        obs_data_set_int(settings, "color", colors[i]);
        obs_data_set_int(settings, "width", 1280);
        obs_data_set_int(settings, "height", 720);
        auto *source = obs_source_create("color_source_v3", sourceName.constData(), settings, nullptr);
        obs_data_release(settings);
        const auto sceneName = QString("MV Smoke Scene %1").arg(i + 1).toUtf8();
        auto *scene = obs_scene_create(sceneName.constData());
        record(QString("fixture_%1").arg(i + 1), source && scene);
        if (source && scene) {
            obs_scene_add(scene, source);
            if (i == 0) obs_frontend_set_current_scene(obs_scene_get_source(scene));
            if (i == 1) {
                obs_frontend_set_preview_program_mode(true);
                obs_frontend_set_current_preview_scene(obs_scene_get_source(scene));
            }
        }
        if (source) obs_source_release(source);
        if (scene) obs_scene_release(scene);
    }
}
void exerciseCollectionReload()
{
    if (qEnvironmentVariableIsSet("MV_TEST_SKIP_COLLECTION")) { finish(); return; }
    char *original = obs_frontend_get_current_scene_collection();
    const QString collection = QString::fromUtf8(original ? original : "");
    bfree(original);
    auto *camera = obs_get_source_by_name("MV Smoke Scene 2");
    const QString uuid = camera ? QString::fromUtf8(obs_source_get_uuid(camera)) : QString();
    obs_source_release(camera);
    obs_frontend_save();
    const bool created = obs_frontend_add_scene_collection("MV Isolated Collection Reload");
    record("collection_test_created", created && !collection.isEmpty() && !uuid.isEmpty());
    if (!created || collection.isEmpty()) { finish(); return; }
    QTimer::singleShot(400, qApp, [collection, uuid] {
        auto *old = obs_get_source_by_uuid(uuid.toUtf8().constData());
        record("collection_old_uuid_unavailable", old == nullptr);
        obs_source_release(old);
        obs_frontend_set_current_scene_collection(collection.toUtf8().constData());
        QTimer::singleShot(600, qApp, [uuid] {
            auto *restored = obs_get_source_by_uuid(uuid.toUtf8().constData());
            record("collection_uuid_restored", restored != nullptr);
            obs_source_release(restored);
            auto *window = findWindow(QString::fromUtf8(mv::WindowTitle));
            bool named = false;
            if (window) for (auto *tile : window->findChildren<QWidget *>("multiviewTile"))
                if (tile->property("nameOverlayText").toString() == "MV Smoke Scene 2") named = true;
            record("collection_tile_resumed", named);
            checkSceneAnchor("collection_reloaded");
            if (window) window->close();
            finish();
        });
    });
}
void exerciseReopen()
{
    checkSceneAnchor("configured");
    auto *window = findWindow(QString::fromUtf8(mv::WindowTitle));
    record("configured_window_visible", window != nullptr);
    capture(window, "configured");
    record("gpu_program_pixels", mvtest::captureGpu(nullptr, true, output + "/gpu-program.png"));
    auto *preview = obs_frontend_get_current_preview_scene();
    record("gpu_preview_pixels", mvtest::captureGpu(preview, false, output + "/gpu-preview.png"));
    obs_source_release(preview);
    for (int i = 0; i < 4; ++i) {
        auto *scene = obs_get_source_by_name(QString("MV Smoke Scene %1").arg(i + 1).toUtf8().constData());
        record(QString("gpu_scene_%1_pixels").arg(i + 1), mvtest::captureGpu(scene, false, output + QString("/gpu-scene-%1.png").arg(i + 1)));
        obs_source_release(scene);
    }
    if (window) {
        for (auto *tile : window->findChildren<QWidget *>("multiviewTile")) {
            QString title;
            for (auto *label : tile->findChildren<QLabel *>())
                if (label->text().startsWith("MV Smoke Scene")) title = label->text();
            if (title == "MV Smoke Scene 2") record("rendered_scene2_red_tally", tile->styleSheet().contains("#ef4444"));
            if (title == "MV Smoke Scene 3") record("rendered_scene3_green_tally", tile->styleSheet().contains("#22c55e"));
            if (title == "MV Smoke Scene 1") record("rendered_hidden_camera_no_tally", !tile->styleSheet().contains("#ef4444") && !tile->styleSheet().contains("#22c55e"));
            if (title == "MV Smoke Scene 2") {
                record("overlay_program_red_background", tile->property("nameOverlayBackground").toString() == "#ef4444");
                record("overlay_program_white_text", tile->property("nameOverlayColor").toString() == "#ffffff");
                record("program_border_uniform", !tile->styleSheet().contains("border-bottom") && !tile->styleSheet().contains("#22c55e"));
                record("overlay_enabled", tile->property("nameOverlayEnabled").toBool());
                record("overlay_name_matches", tile->property("nameOverlayText").toString() == title);
                bool stripHidden = true;
                for (auto *label : tile->findChildren<QLabel *>())
                    if (label->text() == title && !label->isHidden()) stripHidden = false;
                record("old_name_strip_removed", stripHidden);
            }
            if (title == "MV Smoke Scene 3") record("overlay_preview_green_background", tile->property("nameOverlayBackground").toString() == "#22c55e" && tile->property("nameOverlayColor").toString() == "#ffffff");
            if (title == "MV Smoke Scene 1") record("overlay_inactive_white", tile->property("nameOverlayColor").toString() == "#ffffff");
            if (title == "MV Smoke Scene 1") record("overlay_inactive_black_background", tile->property("nameOverlayBackground").toString() == "#000000");
            if (tile->property("clockText").isValid()) record("clock_displays_local_time", !tile->property("clockText").toString().isEmpty());
            if (tile->property("resourceText").isValid()) record("resource_displays_values_or_unavailable", tile->property("resourceText").toString().contains("CPU") && tile->property("resourceText").toString().contains("GPU"));
        }
        for (auto *action : window->findChildren<QAction *>())
            if (action->text() == QStringLiteral("Vollbild")) { action->trigger(); break; }
        record("fullscreen_enter", window->isFullScreen());
        auto *identity = window->findChild<QLabel *>("creatorVersionLabel");
        record("creator_version_fullscreen_visible", identity && identity->isVisible() && identity->text() == QString::fromUtf8(mv::Identity));
        QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(window, &escape);
        record("fullscreen_escape", !window->isFullScreen());
    }
    record("clock_tile_present", results.value("clock_displays_local_time").toBool());
    record("resource_tile_present", results.value("resource_displays_values_or_unavailable").toBool());
    auto *samePreview = obs_get_source_by_name("MV Smoke Scene 2");
    obs_frontend_set_current_preview_scene(samePreview);
    obs_source_release(samePreview);
    if (window) window->close();
    record("close_hidden", findWindow(QString::fromUtf8(mv::WindowTitle)) == nullptr);
    QTimer::singleShot(300, qApp, [] {
        if (launchAction) launchAction->trigger();
        QTimer::singleShot(1000, qApp, [] {
            auto *reopened = findWindow(QString::fromUtf8(mv::WindowTitle));
            record("reopen_visible", reopened != nullptr);
            capture(reopened, "reopened");
            checkSceneAnchor("reopened");
            bool bothRed = false;
            if (reopened) for (auto *tile : reopened->findChildren<QWidget *>("multiviewTile")) {
                if (tile->property("nameOverlayText").toString() == "MV Smoke Scene 2")
                    bothRed = tile->property("nameOverlayBackground").toString() == "#ef4444" &&
                        tile->property("nameOverlayColor").toString() == "#ffffff" &&
                        tile->styleSheet().contains("#ef4444") && !tile->styleSheet().contains("#22c55e");
            }
            record("simultaneous_pgm_pvw_program_priority", bothRed);
            if (reopened) reopened->close();
            record("second_close_hidden", findWindow(QString::fromUtf8(mv::WindowTitle)) == nullptr);
            if (launchAction) launchAction->trigger();
            QTimer::singleShot(300, qApp, exerciseCollectionReload);
        });
    });
}
void editSettings()
{
    checkSceneAnchor("initial");
    auto *window = findWindow(QString::fromUtf8(mv::WindowTitle));
    record("multiview_visible", window != nullptr);
    capture(window, "initial");
    if (!window) { finish(); return; }
    QAction *settings = nullptr;
    for (auto *action : window->findChildren<QAction *>())
        if (action->text() == QStringLiteral("Layout-Einstellungen")) { settings = action; break; }
    record("settings_action", settings != nullptr);
    if (!settings) { finish(); return; }
    // Install timer first: QAction enters QDialog::exec synchronously.
    QTimer::singleShot(600, qApp, [] {
        auto *dialog = qobject_cast<QDialog *>(findWindow(QStringLiteral("Multiview-Einstellungen")));
        record("settings_dialog", dialog != nullptr);
        auto *identity = dialog ? dialog->findChild<QLabel *>("creatorVersionLabel") : nullptr;
        record("creator_version_settings_visible", identity && identity->isVisible() && identity->text() == QString::fromUtf8(mv::Identity));
        if (!dialog) { finish(); return; }
        auto *table = dialog->findChild<QTableWidget *>();
        record("settings_table_default_10", table && table->rowCount() == 10);
        record("settings_four_columns", table && table->columnCount() == 4);
        record("no_coordinate_spinboxes", dialog->findChildren<QSpinBox *>().isEmpty());
        if (table) {
            auto *kind = qobject_cast<QComboBox *>(table->cellWidget(0, 0));
            record("source_kind_preserved", kind && kind->findText(QStringLiteral("Quelle")) >= 0);
        }
        bool removedPresetsAbsent = true;
        for (auto *box : dialog->findChildren<QComboBox *>())
            if (box->findData("focus4") >= 0 || box->findData("focus8") >= 0) removedPresetsAbsent = false;
        record("old_focus_presets_removed", removedPresetsAbsent);
        if (table && table->rowCount() >= 7) {
            for (int i = 0; i < 4; ++i) {
                auto *kind = qobject_cast<QComboBox *>(table->cellWidget(i + 2, 0));
                auto *source = qobject_cast<QComboBox *>(table->cellWidget(i + 2, 1));
                if (!kind || !source) { record("settings_controls", false); continue; }
                kind->setCurrentIndex(2); // Scene enum.
                const int index = source->findText(QString("MV Smoke Scene %1").arg(i + 1));
                record(QString("scene_choice_%1").arg(i + 1), index >= 0);
                if (index >= 0) source->setCurrentIndex(index);
            }
            auto *statsKind = qobject_cast<QComboBox *>(table->cellWidget(6, 0));
            if (statsKind) statsKind->setCurrentIndex(4);
            if (table->rowCount() >= 9) {
                qobject_cast<QComboBox *>(table->cellWidget(7, 0))->setCurrentIndex(6);
                qobject_cast<QComboBox *>(table->cellWidget(8, 0))->setCurrentIndex(7);
            }
        }
        capture(dialog, "settings");
        auto *preview = dialog->findChild<QWidget *>("layoutPreview");
        record("drag_preview_present", preview != nullptr);
        if (preview) {
            QRect from, to;
            bool haveFrom = QMetaObject::invokeMethod(preview, "tileRect", Qt::DirectConnection, Q_RETURN_ARG(QRect, from), Q_ARG(int, 0));
            bool haveTo = QMetaObject::invokeMethod(preview, "tileRect", Qt::DirectConnection, Q_RETURN_ARG(QRect, to), Q_ARG(int, 2));
            record("drag_rects_available", haveFrom && haveTo && !from.isEmpty() && !to.isEmpty());
            if (haveFrom && haveTo) {
                const QPointF start = from.center(), end = to.center();
                QMouseEvent press(QEvent::MouseButtonPress, start, preview->mapToGlobal(start.toPoint()), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QMouseEvent move(QEvent::MouseMove, end, preview->mapToGlobal(end.toPoint()), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
                QMouseEvent release(QEvent::MouseButtonRelease, end, preview->mapToGlobal(end.toPoint()), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(preview, &press);
                QApplication::sendEvent(preview, &move);
                QApplication::sendEvent(preview, &release);
                QRect after;
                QMetaObject::invokeMethod(preview, "tileRect", Qt::DirectConnection, Q_RETURN_ARG(QRect, after), Q_ARG(int, 0));
                record("drag_large_pair_moved_down", after.center().y() > from.center().y());
                capture(dialog, "settings-dragged");
            }
        }
        dialog->accept();
    });
    settings->trigger();
    // Verify actual persisted positions, not just the preview's drawing.
    const QString configPath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../../config/obs-studio/plugin_config/obs-multiview-plus/layout.json");
    QFile saved(configPath);
    if (saved.open(QIODevice::ReadOnly)) {
        const auto cells = QJsonDocument::fromJson(saved.readAll()).object()["tiles"].toArray();
        record("drag_saved_large_bottom", cells.size() == 10 && cells[0].toObject()["y"].toInt() == 2 && cells[1].toObject()["y"].toInt() == 2);
        record("drag_saved_small_top", cells.size() == 10 && cells[2].toObject()["y"].toInt() == 0 && cells[6].toObject()["y"].toInt() == 1);
    } else record("drag_settings_saved", false);
    if (!finished) QTimer::singleShot(4500, qApp, exerciseReopen);
}
void startLegacyChecks()
{
    createScenes();
    exerciseTallyGraphs();
    auto *mainWindow = static_cast<QWidget *>(obs_frontend_get_main_window());
    if (mainWindow) for (auto *action : mainWindow->findChildren<QAction *>())
        if (action->text() == "Sunjoo OBS Link Multiview") { launchAction = action; break; }
    record("tools_action", launchAction != nullptr);
    if (!launchAction) { finish(); return; }
    launchAction->trigger();
    QTimer::singleShot(2000, qApp, editSettings);
}
void start()
{
    if (started) return;
    started = true;
    if (qEnvironmentVariableIsSet("MV_TEST_SKIP_CONTROLLER")) { startLegacyChecks(); return; }
    mvtest::runControllerFixture(output, record, [] {
        mvtest::runSwitcherFixture(output, record, startLegacyChecks);
    });
}
void eventCallback(enum obs_frontend_event event, void *)
{
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
        QTimer::singleShot(1000, qApp, start);
}
}
bool obs_module_load(void)
{
    output = qEnvironmentVariable("MV_SMOKE_OUTPUT");
    const auto args = QCoreApplication::arguments();
    const bool portable = args.contains("--portable") || args.contains("-p");
    if (output.isEmpty() || !portable || !QDir::isAbsolutePath(output) || !QDir().mkpath(output)) {
        blog(LOG_WARNING, "[mv-smoke] disabled: requires explicit portable flag and absolute MV_SMOKE_OUTPUT");
        return false;
    }
    blog(LOG_INFO, "[mv-smoke] test-only driver enabled in explicitly portable process");
    return true;
}
void obs_module_post_load(void)
{
    obs_frontend_add_event_callback(eventCallback, nullptr);
    QTimer::singleShot(30000, qApp, [] {
        if (!finished) { record("watchdog_timeout", false); finish(); }
    });
}
void obs_module_unload(void) { obs_frontend_remove_event_callback(eventCallback, nullptr); }
