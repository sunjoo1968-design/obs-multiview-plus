// Test-only differential lifetime probe; never installed or distributed.
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/bmem.h>
#include <QApplication>
#include <QAction>
#include <QTimer>
#include <QWidget>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QJsonArray>

OBS_DECLARE_MODULE()
namespace {
QString output, mode;
QJsonObject result;
bool started = false;
QAction *launch = nullptr;
int cycles = 0, completed = 0;
QJsonArray samples;
void finish()
{
    result["passed"] = mode != "window" || (result["opened"].toBool() && completed == cycles);
    result["completed_cycles"] = completed;
    result["closed_allocation_samples"] = samples;
    result["before_shutdown_allocations"] = int(bnum_allocs());
    QFile file(output + "/result.json");
    if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(result).toJson());
    file.close();
    static_cast<QWidget *>(obs_frontend_get_main_window())->close();
}
void cycle()
{
    launch->trigger();
    QTimer::singleShot(300, qApp, [] {
        bool closed = false;
        for (auto *widget : QApplication::topLevelWidgets())
            if (widget->windowTitle().startsWith("OBS Multiview Plus") && widget->isVisible()) {
                widget->close(); closed = true;
            }
        if (!closed) { result["opened"] = false; finish(); return; }
        QTimer::singleShot(200, qApp, [] {
            samples.append(int(bnum_allocs()));
            if (++completed < cycles) cycle(); else finish();
        });
    });
}
void start()
{
    if (started) return;
    started = true;
    result["mode"] = mode;
    result["initial_allocations"] = int(bnum_allocs());
    if (mode == "window") {
        auto *main = static_cast<QWidget *>(obs_frontend_get_main_window());
        bool opened = false;
        for (auto *action : main->findChildren<QAction *>())
            if (action->text() == "Multiview Plus") { launch = action; opened = true; break; }
        result["opened"] = opened;
        cycles = qEnvironmentVariableIntValue("MV_LEAK_CYCLES");
        if (cycles < 1 || cycles > 40) cycles = 1;
        if (opened) { cycle(); return; }
    }
    QTimer::singleShot(2500, qApp, finish);
}
void event(obs_frontend_event value, void *)
{
    if (value == OBS_FRONTEND_EVENT_FINISHED_LOADING) QTimer::singleShot(500, qApp, start);
}
}
bool obs_module_load()
{
    output = qEnvironmentVariable("MV_SMOKE_OUTPUT");
    mode = qEnvironmentVariable("MV_LEAK_MODE");
    return !output.isEmpty() && QDir::isAbsolutePath(output) &&
        QCoreApplication::arguments().contains("--portable") && !mode.isEmpty();
}
void obs_module_post_load() { obs_frontend_add_event_callback(event, nullptr); }
void obs_module_unload() { obs_frontend_remove_event_callback(event, nullptr); }
