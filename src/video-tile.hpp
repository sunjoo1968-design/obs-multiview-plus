#pragma once

#include "layout-model.hpp"
#include <QWidget>
#include <QImage>
#include <obs.h>
#include <obs-frontend-api.h>
#include <mutex>
#include <memory>

class QLabel;
class QTimer;

namespace mv {
class ResourceMonitor;
class VideoTile final : public QWidget {
public:
    VideoTile(const TileConfig &config, bool showNames, QWidget *parent = nullptr);
    ~VideoTile() override;
    void setClickOptions(bool single, bool doubleTransition);
    void suspend();
    void resume();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    static void draw(void *data, uint32_t cx, uint32_t cy);
    static void frontendEvent(obs_frontend_event event, void *data);
    void refresh();
    void createDisplay();
    void releaseSource();
    void switchScene(bool doubleClick);
    void updateStats();
    void updateNameOverlay();
    void drawNameOverlay(uint32_t cx, uint32_t cy);

    TileConfig config_;
    QWidget *surface_ = nullptr;
    QLabel *name_ = nullptr;
    QLabel *message_ = nullptr;
    QTimer *timer_ = nullptr;
    std::shared_ptr<ResourceMonitor> resourceMonitor_;
    obs_display_t *display_ = nullptr;
    // The mutex protects the render snapshot; OBS references never outlive it.
    std::mutex mutex_;
    QImage nameImage_;
    bool nameImageDirty_ = false;
    gs_texture_t *nameTexture_ = nullptr;
    QString nameImageKey_;
    obs_source_t *source_ = nullptr;
    bool renderProgram_ = false;
    bool suspended_ = false;
    bool singleClick_ = false;
    bool doubleTransition_ = false;
    bool showNames_ = true;
    int tallyStyle_ = -1;
    bool smokeLog_ = false;
    uint32_t loggedDrawWidth_ = 0;
    uint32_t loggedDrawHeight_ = 0;
    unsigned loggedDrawCount_ = 0;
    unsigned loggedCreateSkips_ = 0;
};
} // namespace mv
