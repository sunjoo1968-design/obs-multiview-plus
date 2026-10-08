#include <obs-module.h>
#include <obs-frontend-api.h>
#include "tally-obs.hpp"
#include "layout-model.hpp"
#include "version.hpp"
#include <QApplication>
#include <QAction>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPointer>
#include <QSaveFile>
#include <QTimer>
#include <QWidget>

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_name(void) { return "TEST ONLY MV Hybrid " MV_VERSION " / SunjooAn"; }
MODULE_EXPORT bool mv_test_source_on_root(obs_source_t *root, obs_source_t *target, obs_source_t *mapping)
{ return mv::sourceOnRoot(root, target, mapping); }
namespace {
QString state;
QPointer<QTimer> timer;
bool exiting = false, opened = false;
void tick()
{
    if (exiting) return;
    if (!opened) {
        auto *module = obs_get_module("obs-multiview-plus");
        auto *main = static_cast<QWidget *>(obs_frontend_get_main_window());
        if (!module || !main) return;
        mv::LayoutConfig config;
        config.columns = 4; config.rows = 2; config.tiles.clear();
        config.tiles.push_back({mv::TileKind::Program, {}, "PGM", {}, 0,0,2,1});
        config.tiles.push_back({mv::TileKind::Preview, {}, "PVW", {}, 2,0,2,1});
        for (int i = 0; i < 4; ++i) {
            const QString name = QString(i < 2 ? "Test Scene %1" : "Test Camera %1").arg(i % 2 + 1);
            auto *source = obs_get_source_by_name(name.toUtf8().constData());
            if (!source) return;
            config.tiles.push_back({i < 2 ? mv::TileKind::Scene : mv::TileKind::Source,
                                   QString::fromUtf8(obs_source_get_uuid(source)), name, {}, i,1,1,1});
            obs_source_release(source);
        }
        char *path = obs_module_get_config_path(module, "layout.json");
        const QString filename = QString::fromUtf8(path); bfree(path);
        QDir().mkpath(QFileInfo(filename).absolutePath());
        QSaveFile file(filename);
        if (!file.open(QIODevice::WriteOnly)) return;
        file.write(QJsonDocument(mv::toJson(config)).toJson()); if (!file.commit()) return;
        for (auto *action : main->findChildren<QAction *>())
            if (action->text() == "Sunjoo OBS Link Multiview") { action->trigger(); opened = true; break; }
    }
    QJsonArray rows;
    for (auto *window : QApplication::topLevelWidgets()) {
        if (window->windowTitle() != QString::fromUtf8(mv::WindowTitle)) continue;
        for (auto *tile : window->findChildren<QWidget *>("multiviewTile")) {
            const auto name = tile->property("nameOverlayText").toString();
            const auto style = tile->styleSheet();
            rows.append(QJsonObject{{"name",name},{"red",style.contains("#ef4444")},{"green",style.contains("#22c55e")}});
        }
    }
    QSaveFile file(state);
    if (file.open(QIODevice::WriteOnly)) { file.write(QJsonDocument(rows).toJson()); file.commit(); }
}
void event(enum obs_frontend_event event, void *)
{
    if (event == OBS_FRONTEND_EVENT_EXIT) { exiting = true; if (timer) timer->stop(); }
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
        QTimer::singleShot(1000,qApp,[]{ timer = new QTimer(qApp); QObject::connect(timer,&QTimer::timeout,qApp,tick);timer->start(200); });
}
}
MODULE_EXPORT bool obs_module_load(void)
{
    state=qEnvironmentVariable("MV_HYBRID_STATE");
    if (state.isEmpty() || !QDir::isAbsolutePath(state) || !QCoreApplication::arguments().contains("--portable")) return false;
    obs_frontend_add_event_callback(event,nullptr); return true;
}
MODULE_EXPORT void obs_module_unload(void)
{
    if (timer) { timer->stop(); delete timer; }
    if (!exiting) obs_frontend_remove_event_callback(event,nullptr);
}
