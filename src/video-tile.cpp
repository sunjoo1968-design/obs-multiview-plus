#include "video-tile.hpp"
#include "tally-obs.hpp"
#include "resource-monitor.hpp"
#include <QDateTime>
#include <cmath>

#include <QLabel>
#include <QMouseEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>
#include <QPlatformSurfaceEvent>
#include <QPainter>
#include <QFontMetrics>
#include <algorithm>

namespace mv {
namespace {
bool isVideo(TileKind kind)
{
    return kind == TileKind::Program || kind == TileKind::Preview || kind == TileKind::Scene || kind == TileKind::Source;
}
class NativeSurface final : public QWidget {
public:
    using QWidget::QWidget;
    QPaintEngine *paintEngine() const override { return nullptr; }
protected:
    void paintEvent(QPaintEvent *) override {}
};

struct SourceRef {
    obs_source_t *value;
    explicit SourceRef(obs_source_t *source) : value(source) {}
    ~SourceRef() { obs_source_release(value); }
    SourceRef(const SourceRef &) = delete;
    SourceRef &operator=(const SourceRef &) = delete;
};

bool onProgram(obs_source_t *target, obs_source_t *explicitSource)
{
    SourceRef output(obs_get_output_source(0));
    if (output.value) return sourceOnRoot(output.value, target, explicitSource);
    SourceRef current(obs_frontend_get_current_scene());
    SourceRef transition(obs_frontend_get_current_transition());
    if (transition.value && obs_transition_is_active(transition.value)) {
        return sourceOnRoot(transition.value, target, explicitSource);
    }
    return sourceOnRoot(current.value, target, explicitSource);
}
} // namespace

VideoTile::VideoTile(const TileConfig &config, bool showNames, QWidget *parent)
    : QWidget(parent), config_(config), showNames_(showNames)
{
    smokeLog_ = qEnvironmentVariableIsSet("MV_SMOKE_OUTPUT");
    setObjectName("multiviewTile");
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumSize(0, 0);
    setStyleSheet("#multiviewTile { background:#080a0c; border:3px solid #363d46; }");
    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetNoConstraint);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(0);
    surface_ = new NativeSurface(this);
    // Stats and empty tiles never acquire a native child window or swap chain.
    if (isVideo(config_.kind)) {
        surface_->setAttribute(Qt::WA_DontCreateNativeAncestors);
        surface_->setAttribute(Qt::WA_StaticContents);
        surface_->setAttribute(Qt::WA_NativeWindow);
        surface_->setAttribute(Qt::WA_PaintOnScreen);
        surface_->setAttribute(Qt::WA_NoSystemBackground);
        surface_->setAttribute(Qt::WA_OpaquePaintEvent);
        surface_->windowHandle()->installEventFilter(this);
        connect(surface_->windowHandle(), &QWindow::screenChanged, this, [this] {
            createDisplay();
            if (display_)
                obs_display_update_color_space(display_);
        });
    }
    surface_->installEventFilter(this);
    layout->addWidget(surface_, 1);
    message_ = new QLabel(this);
    message_->setAlignment(Qt::AlignCenter);
    message_->setWordWrap(true);
    message_->setTextFormat(Qt::PlainText);
    message_->setStyleSheet("color:#aeb9c6; background:#080a0c; border:none; padding:8px;");
    layout->addWidget(message_, 1);
    name_ = new QLabel(this);
    name_->setAlignment(Qt::AlignCenter);
    name_->setStyleSheet("color:white; background:#171c23; border:none; padding:4px;");
    name_->setTextFormat(Qt::PlainText);
    name_->hide(); // Metadata only; the name is composited into the video.
    name_->installEventFilter(this);
    timer_ = new QTimer(this);
    timer_->setInterval(isVideo(config_.kind) ? 200 : 1000);
    connect(timer_, &QTimer::timeout, this, [this] { refresh(); });
    obs_frontend_add_event_callback(frontendEvent, this);
    surface_->hide();
    message_->setText(config_.kind == TileKind::Empty ? QString() : QStringLiteral("Lädt …"));
}

VideoTile::~VideoTile()
{
    obs_frontend_remove_event_callback(frontendEvent, this);
    timer_->stop();
    destroyDisplay();
    releaseSource();
    obs_enter_graphics();
    gs_texture_destroy(nameTexture_);
    nameTexture_ = nullptr;
    obs_leave_graphics();
}

void VideoTile::setClickOptions(bool single, bool doubleTransition)
{
    singleClick_ = single;
    doubleTransition_ = doubleTransition;
    surface_->setCursor((single || doubleTransition) && config_.kind == TileKind::Scene ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void VideoTile::releaseSource()
{
    obs_source_t *previous;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        previous = source_;
        source_ = nullptr;
        renderProgram_ = false;
    }
    if (previous) {
        obs_source_dec_showing(previous);
        obs_source_release(previous);
    }
}

void VideoTile::suspend()
{
    resourceMonitor_.reset();
    suspended_ = true;
    timer_->stop();
    if (display_)
        obs_display_set_enabled(display_, false);
    releaseSource();
}

void VideoTile::resume()
{
    suspended_ = false;
    if (isVisible()) {
        refresh();
        createDisplay();
        timer_->start();
    }
}

void VideoTile::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!suspended_) {
        refresh();
        timer_->start();
        QTimer::singleShot(0, this, [this] { createDisplay(); });
    }
}

void VideoTile::hideEvent(QHideEvent *event)
{
    resourceMonitor_.reset();
    timer_->stop();
    if (display_)
        obs_display_set_enabled(display_, false);
    releaseSource();
    QWidget::hideEvent(event);
}

void VideoTile::destroyDisplay()
{
    if (!display_)
        return;
    obs_display_remove_draw_callback(display_, draw, this);
    obs_display_destroy(display_);
    display_ = nullptr;
    displayWidth_ = 0;
    displayHeight_ = 0;
}

// Keeps the swap chain at the surface's pixel size. This deliberately does not
// depend on the window being exposed: during the macOS full-screen animation the
// final resize can arrive while the window is not exposed, and a skipped resize
// leaves the video taller than its tile, covering tally borders and neighbours.
void VideoTile::syncDisplaySize()
{
    if (!display_)
        return;
    const auto dpr = surface_->devicePixelRatioF();
    const uint32_t width = uint32_t(std::max(1, qRound(surface_->width() * dpr)));
    const uint32_t height = uint32_t(std::max(1, qRound(surface_->height() * dpr)));
    if (width == displayWidth_ && height == displayHeight_)
        return;
    displayWidth_ = width;
    displayHeight_ = height;
    obs_display_resize(display_, width, height);
}

void VideoTile::createDisplay()
{
    syncDisplaySize();
    if (smokeLog_ && !display_ && loggedCreateSkips_++ < 3)
        blog(LOG_INFO, "[mv-display-attempt] tile=%p suspended=%d visible=%d surfaceVisible=%d handle=%p exposed=%d surface=%dx%d",
             static_cast<void *>(this), int(suspended_), int(isVisible()), int(surface_->isVisible()),
             static_cast<void *>(surface_->windowHandle()), int(surface_->windowHandle() && surface_->windowHandle()->isExposed()),
             surface_->width(), surface_->height());
    if (suspended_ || !surface_->isVisible() || !isVisible())
        return;
    if (!surface_->windowHandle() || !surface_->windowHandle()->isExposed())
        return;
    const auto dpr = surface_->devicePixelRatioF();
    uint32_t width = uint32_t(std::max(1, qRound(surface_->width() * dpr)));
    uint32_t height = uint32_t(std::max(1, qRound(surface_->height() * dpr)));
    if (!display_) {
        gs_init_data info{};
        info.cx = width;
        info.cy = height;
        info.format = GS_BGRA;
        info.zsformat = GS_ZS_NONE;
        void *const nativeHandle = reinterpret_cast<void *>(surface_->winId());
#if defined(_WIN32)
        info.window.hwnd = nativeHandle;
#elif defined(__APPLE__)
        // On macOS, winId() is the NSView that backs the native child widget.
        info.window.view = reinterpret_cast<id>(nativeHandle);
#else
#error "Only Windows and macOS are supported"
#endif
        display_ = obs_display_create(&info, 0xFF080A0C);
        if (smokeLog_)
            blog(LOG_INFO, "[mv-display] tile=%p kind=%d hwnd=%p create=%p size=%ux%u rect=%d,%d,%d,%d exposed=%d",
                 static_cast<void *>(this), int(config_.kind), nativeHandle, static_cast<void *>(display_), width, height,
                 surface_->x(), surface_->y(), surface_->width(), surface_->height(), int(surface_->windowHandle()->isExposed()));
        if (display_) {
            displayWidth_ = width;
            displayHeight_ = height;
            obs_display_add_draw_callback(display_, draw, this);
        } else {
            message_->setText(QStringLiteral("Videoanzeige konnte nicht erstellt werden"));
            message_->show();
            blog(LOG_ERROR, "[obs-multiview] Failed to create tile display");
        }
    }
    if (display_)
        obs_display_set_enabled(display_, true);
}

bool VideoTile::eventFilter(QObject *watched, QEvent *event)
{
    // The native view behind the surface is going away (window closed, moved to
    // another parent, ...). The OBS display must not outlive it, otherwise OBS
    // renders into a dangling view and crashes; it is recreated on the next show.
    if (watched == surface_->windowHandle() && event->type() == QEvent::PlatformSurface &&
        static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType() ==
            QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
        destroyDisplay();
    if (watched == surface_ && event->type() == QEvent::Resize)
        syncDisplaySize();
    if (watched == surface_->windowHandle() && event->type() == QEvent::Expose)
        QTimer::singleShot(0, this, [this] { createDisplay(); });
    if (watched == surface_ && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
        QTimer::singleShot(0, this, [this] { updateNameOverlay(); createDisplay(); });
    if ((watched == surface_ || watched == name_) &&
        (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            switchScene(event->type() == QEvent::MouseButtonDblClick);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void VideoTile::switchScene(bool doubleClick)
{
    if (suspended_ || config_.kind != TileKind::Scene ||
        (doubleClick ? !doubleTransition_ : !singleClick_))
        return;
    SourceRef source(obs_get_source_by_uuid(config_.uuid.toUtf8().constData()));
    if (!source.value || !obs_source_is_scene(source.value) || obs_source_removed(source.value))
        return;
    const bool studio = obs_frontend_preview_program_mode_active();
    if (doubleClick) {
        if (studio) {
            obs_frontend_set_current_preview_scene(source.value);
            obs_frontend_preview_program_trigger_transition();
        } else {
            obs_frontend_set_current_scene(source.value);
        }
    } else if (studio) {
        obs_frontend_set_current_preview_scene(source.value);
    } else {
        obs_frontend_set_current_scene(source.value);
    }
}

void VideoTile::frontendEvent(obs_frontend_event event, void *data)
{
    auto *tile = static_cast<VideoTile *>(data);
    // Frontend events run on the OBS GUI thread. Cleanup must release references
    // before OBS destroys the old scene collection, so do not queue this path.
    switch (event) {
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
    case OBS_FRONTEND_EVENT_EXIT:
        tile->suspend();
        break;
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
    case OBS_FRONTEND_EVENT_FINISHED_LOADING:
        tile->resume();
        break;
    default:
        break;
    }
}

void VideoTile::refresh()
{
    if (suspended_ || !isVisible())
        return;
    const bool stats = config_.kind == TileKind::Stats;
    const bool empty = config_.kind == TileKind::Empty;
    QString title;
    bool program = false;
    obs_source_t *next = nullptr;
    switch (config_.kind) {
    case TileKind::Program:
        title = QStringLiteral("PGM");
        program = true;
        break;
    case TileKind::Preview:
        title = QStringLiteral("PVW");
        if (obs_frontend_preview_program_mode_active())
            next = obs_frontend_get_current_preview_scene();
        else
            program = true;
        break;
    case TileKind::Scene:
    case TileKind::Source:
        next = obs_get_source_by_uuid(config_.uuid.toUtf8().constData());
        if (next && obs_source_removed(next)) {
            obs_source_release(next);
            next = nullptr;
        }
        title = next ? QString::fromUtf8(obs_source_get_name(next)) : QStringLiteral("Kein Ziel");
        break;
    case TileKind::Stats: title = QStringLiteral("OBS-Statistik"); break;
    case TileKind::Clock: title = QStringLiteral("Uhr"); break;
    case TileKind::Resources: title = QStringLiteral("CPU / GPU (OBS)"); break;
    case TileKind::Empty: title = QStringLiteral("Leer"); break;
    }
    bool red = config_.kind == TileKind::Program;
    bool green = config_.kind == TileKind::Preview && obs_frontend_preview_program_mode_active();
    if (next && (config_.kind == TileKind::Scene || config_.kind == TileKind::Source)) {
        SourceRef explicitSource(config_.tallyUuid.isEmpty() ? nullptr :
            obs_get_source_by_uuid(config_.tallyUuid.toUtf8().constData()));
        const bool validTally = config_.tallyUuid.isEmpty() ||
            (explicitSource.value && !obs_source_removed(explicitSource.value));
        red = validTally && onProgram(next, explicitSource.value);
        if (validTally && obs_frontend_preview_program_mode_active()) {
            SourceRef preview(obs_frontend_get_current_preview_scene());
            green = sourceOnRoot(preview.value, next, explicitSource.value);
        }
    }
    // Release outside the render mutex: source destruction may enter graphics.
    // The GUI thread owns source changes. Avoid repeated showing signals for
    // an unchanged source; release the temporary lookup reference instead.
    const bool sourceChanged = next != source_;
    if (sourceChanged && next)
        obs_source_inc_showing(next);
    obs_source_t *previous;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        previous = sourceChanged ? source_ : nullptr;
        if (sourceChanged) source_ = next;
        renderProgram_ = program;
    }
    if (previous) {
        obs_source_dec_showing(previous);
        obs_source_release(previous);
    }
    if (!sourceChanged && next) obs_source_release(next);
    const bool video = isVideo(config_.kind) && (program || next);
    surface_->setVisible(video);
    if (video && !display_)
        createDisplay();
    else if (video)
        syncDisplaySize(); // Self-healing if a resize was missed.
    message_->setVisible(!video);
    if (!video) {
        if (display_)
            obs_display_set_enabled(display_, false);
        if (stats)
            updateStats();
        else if (config_.kind == TileKind::Clock) {
            const auto now = QDateTime::currentDateTime();
            message_->setText((config_.label.isEmpty() ? QString() : config_.label + "\n") + now.toString("yyyy-MM-dd\nhh:mm:ss"));
            setProperty("clockText", now.toString(Qt::ISODate));
        } else if (config_.kind == TileKind::Resources) {
            if (!resourceMonitor_) resourceMonitor_ = ResourceMonitor::acquire();
            const auto sample = resourceMonitor_ ? resourceMonitor_->snapshot() : ResourceSnapshot{};
            const auto percent = [](double value) { return std::isfinite(value) ? QString::number(value, 'f', 1) + "%" : QStringLiteral("nicht messbar"); };
            message_->setText((config_.label.isEmpty() ? QStringLiteral("OBS-Auslastung") : config_.label) +
                QStringLiteral("\nCPU %1\nGPU %2\nGPU: meistbelastete Engine").arg(percent(sample.cpuPercent), percent(sample.gpuPercent)));
            setProperty("resourceText", message_->text());
        }
        else
            message_->setText(empty ? QString() : QStringLiteral("Bitte Szene oder Quelle auswählen\nGelöschte Ziele in den Einstellungen neu festlegen"));
    }
    name_->setText(config_.label.isEmpty() ? title : config_.label);
    name_->setToolTip(title);
    const int style = (red ? 1 : 0) | (green ? 2 : 0);
    if (style != tallyStyle_) {
        tallyStyle_ = style;
        const QString color = red ? QStringLiteral("#ef4444") : green ? QStringLiteral("#22c55e") : QStringLiteral("#363d46");
        // Program takes precedence when also present in preview.
        setStyleSheet(QStringLiteral("#multiviewTile { background:#080a0c; border:3px solid %1; }").arg(color));
    }
    updateNameOverlay();
}

void VideoTile::updateNameOverlay()
{
    const bool enabled = showNames_ && isVideo(config_.kind);
    const QString color = QStringLiteral("#ffffff");
    const QString background = (tallyStyle_ & 1) ? QStringLiteral("#ef4444") :
        (tallyStyle_ & 2) ? QStringLiteral("#22c55e") : QStringLiteral("#000000");
    setProperty("nameOverlayText", name_->text());
    setProperty("nameOverlayColor", color);
    setProperty("nameOverlayBackground", background);
    setProperty("nameOverlayEnabled", enabled);
    const qreal dpr = surface_->devicePixelRatioF();
    const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(name_->text()).arg(tallyStyle_).arg(enabled).arg(surface_->width()).arg(surface_->height()).arg(dpr);
    if (key == nameImageKey_)
        return;
    nameImageKey_ = key;
    QImage image;
    const int available = surface_->width() - 16;
    if (enabled && available > 16 && surface_->height() > 24 && !name_->text().isEmpty()) {
        QFont font = this->font();
        font.setPixelSize(std::clamp(surface_->height() / 15, 12, 22));
        font.setBold(true);
        const QFontMetrics metrics(font);
        const QString text = metrics.elidedText(name_->text(), Qt::ElideRight, available - 16);
        const int width = std::min(available, metrics.horizontalAdvance(text) + 16);
        const int height = metrics.height() + 8;
        image = QImage(std::max(1, qRound(width * dpr)), std::max(1, qRound(height * dpr)), QImage::Format_RGBA8888);
        image.setDevicePixelRatio(dpr);
        image.fill(QColor(background));
        QPainter painter(&image);
        painter.setFont(font);
        painter.setPen(QColor(color));
        painter.drawText(QRect(8, 4, width - 16, metrics.height()), Qt::AlignCenter, text);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    nameImage_ = std::move(image);
    nameImageDirty_ = true;
}

// Called only by the display draw callback, with graphics context and mutex held.
void VideoTile::drawNameOverlay(uint32_t cx, uint32_t cy)
{
    if (nameImageDirty_) {
        gs_texture_destroy(nameTexture_);
        nameTexture_ = nullptr;
        if (!nameImage_.isNull()) {
            const uint8_t *pixels = nameImage_.constBits();
            nameTexture_ = gs_texture_create(uint32_t(nameImage_.width()), uint32_t(nameImage_.height()), GS_RGBA, 1, &pixels, 0);
        }
        nameImageDirty_ = false;
    }
    if (!nameTexture_ || !cx || !cy)
        return;
    const uint32_t width = gs_texture_get_width(nameTexture_);
    const uint32_t height = gs_texture_get_height(nameTexture_);
    const float margin = float(6 * nameImage_.devicePixelRatio());
    gs_viewport_push();
    gs_projection_push();
    gs_matrix_push();
    gs_blend_state_push();
    gs_enable_blending(true);
    gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);
    gs_set_viewport(0, 0, int(cx), int(cy));
    gs_ortho(0, float(cx), 0, float(cy), -100, 100);
    gs_matrix_identity();
    gs_matrix_translate3f(std::max(0.0f, (float(cx) - width) / 2), std::max(0.0f, float(cy) - height - margin), 0);
    gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
    gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), nameTexture_);
    while (gs_effect_loop(effect, "Draw"))
        gs_draw_sprite(nameTexture_, 0, width, height);
    gs_blend_state_pop();
    gs_matrix_pop();
    gs_projection_pop();
    gs_viewport_pop();
}

void VideoTile::updateStats()
{
    const auto frames = obs_get_total_frames();
    const auto lagged = obs_get_lagged_frames();
    QString text = QStringLiteral("%1 FPS\nVerzögerte Frames %2 / %3\nDurchschn. Renderzeit %4 ms")
        .arg(obs_get_active_fps(), 0, 'f', 2).arg(lagged).arg(frames)
        .arg(double(obs_get_average_frame_time_ns()) / 1000000.0, 0, 'f', 2);
    obs_output_t *output = obs_frontend_get_streaming_output();
    if (output) {
        text += QStringLiteral("\nStream %1\nNetzwerk-Drops %2")
            .arg(obs_output_active(output) ? QStringLiteral("läuft") : QStringLiteral("wartet"))
            .arg(obs_output_get_frames_dropped(output));
        obs_output_release(output);
    }
    if (showNames_)
        text.prepend((config_.label.isEmpty() ? QStringLiteral("OBS-Statistik") : config_.label) + QStringLiteral("\n"));
    message_->setText(text);
}

void VideoTile::draw(void *data, uint32_t cx, uint32_t cy)
{
    auto *tile = static_cast<VideoTile *>(data);
    std::lock_guard<std::mutex> lock(tile->mutex_);
    if (tile->smokeLog_ && (tile->loggedDrawCount_ < 2 || tile->loggedDrawWidth_ != cx || tile->loggedDrawHeight_ != cy)) {
        tile->loggedDrawWidth_ = cx;
        tile->loggedDrawHeight_ = cy;
        ++tile->loggedDrawCount_;
        blog(LOG_INFO, "[mv-draw] tile=%p size=%ux%u main=%d source=%s sourceSize=%ux%u",
             static_cast<void *>(tile), cx, cy, int(tile->renderProgram_),
             tile->source_ ? obs_source_get_name(tile->source_) : "(null)",
             tile->source_ ? obs_source_get_width(tile->source_) : 0,
             tile->source_ ? obs_source_get_height(tile->source_) : 0);
    }
    if (!tile->renderProgram_ && !tile->source_)
        return;
    uint32_t width = 0, height = 0;
    if (tile->renderProgram_) {
        obs_video_info info{};
        if (!obs_get_video_info(&info))
            return;
        width = info.base_width;
        height = info.base_height;
    } else {
        width = obs_source_get_width(tile->source_);
        height = obs_source_get_height(tile->source_);
    }
    if (!width || !height || !cx || !cy) {
        tile->drawNameOverlay(cx, cy);
        return;
    }
    const float scale = std::min(float(cx) / float(width), float(cy) / float(height));
    const int w = std::max(1, int(float(width) * scale));
    const int h = std::max(1, int(float(height) * scale));
    gs_viewport_push();
    gs_projection_push();
    gs_matrix_push();
    gs_matrix_identity();
    gs_set_viewport((int(cx) - w) / 2, (int(cy) - h) / 2, w, h);
    gs_ortho(0.0f, float(width), 0.0f, float(height), -100.0f, 100.0f);
    if (tile->renderProgram_)
        obs_render_main_texture();
    else
        obs_source_video_render(tile->source_);
    gs_matrix_pop();
    gs_projection_pop();
    gs_viewport_pop();
    tile->drawNameOverlay(cx, cy);
}
} // namespace mv
