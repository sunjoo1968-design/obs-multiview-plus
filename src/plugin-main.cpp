#include "layout-model.hpp"
#include "settings-dialog.hpp"
#include "video-tile.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/bmem.h>

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QSaveFile>
#include <QScreen>
#include <QToolBar>

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Custom native multiview with flexible layouts and visibility tally";
}
MODULE_EXPORT const char *obs_module_name(void) { return "OBS Multiview Plus 0.5.0 Controller"; }

namespace {
QString settingsPath()
{
	char *path = obs_module_config_path("layout.json");
	QString result = QString::fromUtf8(path ? path : "");
	bfree(path);
	return result;
}

bool writeLayout(const QString &path, const mv::LayoutConfig &config)
{
	if (path.isEmpty() || !QDir().mkpath(QFileInfo(path).absolutePath()))
		return false;
	QSaveFile file(path);
	const QByteArray bytes = QJsonDocument(mv::toJson(config)).toJson();
	return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

bool readLayout(const QString &path, mv::LayoutConfig &config, QString *error)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		*error = file.errorString();
		return false;
	}
	if (file.size() > 1024 * 1024) {
		*error = QStringLiteral("설정 파일이 너무 큽니다.");
		return false;
	}
	QJsonParseError parseError;
	const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
		*error = QStringLiteral("올바른 JSON 설정 파일이 아닙니다.");
		return false;
	}
	return mv::fromJson(doc.object(), config, error);
}

class Canvas final : public QWidget {
public:
	explicit Canvas(QWidget *parent) : QWidget(parent)
	{
		setStyleSheet(QStringLiteral("background: #080a0c;"));
		setMinimumSize(240, 240);
	}
	void apply(const mv::LayoutConfig &config)
	{
		for (auto *tile : tiles)
			delete tile;
		tiles.clear();
		layout = config;
		for (const auto &entry : layout.tiles) {
			auto *tile = new mv::VideoTile(entry, layout.showNames, this);
			tile->setClickOptions(layout.clickSwitch, layout.doubleClickTransition);
			tiles.push_back(tile);
			tile->show();
		}
		arrange();
	}
	void suspend() { for (auto *tile : tiles) tile->suspend(); }
	void resume() { for (auto *tile : tiles) tile->resume(); }
protected:
	void resizeEvent(QResizeEvent *event) override { QWidget::resizeEvent(event); arrange(); }
private:
	void arrange()
	{
		const double aspect = layout.portrait ? 9.0 / 16.0 : 16.0 / 9.0;
		int cw = width();
		int ch = qRound(cw / aspect);
		if (ch > height()) { ch = height(); cw = qRound(ch * aspect); }
		const int ox = (width() - cw) / 2, oy = (height() - ch) / 2;
		for (int i = 0; i < tiles.size(); ++i) {
			const auto &cell = layout.tiles[i];
			int left = ox + cell.x * cw / layout.columns;
			int top = oy + cell.y * ch / layout.rows;
			int right = ox + (cell.x + cell.w) * cw / layout.columns;
			int bottom = oy + (cell.y + cell.h) * ch / layout.rows;
			tiles[i]->setGeometry(left, top, right - left, bottom - top);
		}
	}
	mv::LayoutConfig layout = mv::defaultLayout();
	QVector<mv::VideoTile *> tiles;
};

class MultiviewWindow final : public QMainWindow {
public:
	MultiviewWindow() : QMainWindow(nullptr)
	{
		setWindowTitle(QStringLiteral("OBS Multiview Plus 0.5.0 Controller"));
		resize(1280, 780);
		config = mv::defaultLayout();
		QString error;
		if (QFile::exists(settingsPath()) && !readLayout(settingsPath(), config, &error))
			blog(LOG_WARNING, "[multiview-plus] Config load failed: %s", error.toUtf8().constData());
		canvas = new Canvas(this);
		setCentralWidget(canvas);
		toolbar = addToolBar(QStringLiteral("멀티뷰"));
		toolbar->setMovable(false);
		auto *settings = toolbar->addAction(QStringLiteral("레이아웃 설정"));
		connect(settings, &QAction::triggered, this, [this] { edit(); });
		auto *save = toolbar->addAction(QStringLiteral("프리셋 내보내기"));
		connect(save, &QAction::triggered, this, [this] {
			auto path = QFileDialog::getSaveFileName(this, QStringLiteral("레이아웃 저장"), "multiview.json", "JSON (*.json)");
			if (!path.isEmpty() && !writeLayout(path, config)) warn(QStringLiteral("설정 파일을 저장하지 못했습니다."));
		});
		auto *load = toolbar->addAction(QStringLiteral("프리셋 가져오기"));
		connect(load, &QAction::triggered, this, [this] {
			auto path = QFileDialog::getOpenFileName(this, QStringLiteral("레이아웃 열기"), {}, "JSON (*.json)");
			if (path.isEmpty()) return;
			auto candidate = config;
			QString error;
			if (!readLayout(path, candidate, &error)) { warn(error); return; }
			apply(candidate);
		});
		toolbar->addSeparator();
		monitors = new QComboBox(toolbar);
		toolbar->addWidget(monitors);
		refreshMonitors();
		auto *fullscreen = toolbar->addAction(QStringLiteral("전체 화면"));
		connect(fullscreen, &QAction::triggered, this, [this] { enterFullscreen(); });
		connect(qApp, &QGuiApplication::screenAdded, this, [this](QScreen *) { refreshMonitors(); });
		connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) {
			if (isFullScreen()) leaveFullscreen();
			refreshMonitors();
		});
		setContextMenuPolicy(Qt::CustomContextMenu);
		connect(this, &QWidget::customContextMenuRequested, this, [this](QPoint pos) {
			QMenu menu(this);
			menu.addAction(QStringLiteral("레이아웃 설정"), this, [this] { edit(); });
			menu.addAction(QStringLiteral("창 모드 (Esc)"), this, [this] { leaveFullscreen(); });
			menu.addAction(QStringLiteral("닫기"), this, [this] { close(); });
			menu.exec(mapToGlobal(pos));
		});
		canvas->apply(config);
	}
	void shutdown() { canvas->suspend(); close(); }
protected:
	void keyPressEvent(QKeyEvent *event) override
	{
		if (event->key() == Qt::Key_Escape && isFullScreen()) { leaveFullscreen(); event->accept(); }
		else QMainWindow::keyPressEvent(event);
	}
	void closeEvent(QCloseEvent *event) override { canvas->suspend(); QMainWindow::closeEvent(event); }
	void showEvent(QShowEvent *event) override { QMainWindow::showEvent(event); canvas->resume(); }
private:
	void warn(const QString &message) { QMessageBox::warning(this, QStringLiteral("멀티뷰 설정"), message); }
	void apply(const mv::LayoutConfig &candidate)
	{
		if (!writeLayout(settingsPath(), candidate)) { warn(QStringLiteral("설정을 저장하지 못했습니다.")); return; }
		config = candidate;
		canvas->apply(config);
	}
	void edit()
	{
		mv::SettingsDialog dialog(this, config);
		if (dialog.exec() == QDialog::Accepted) apply(dialog.resultConfig());
	}
	void refreshMonitors()
	{
		QString previous = monitors->currentData().toString();
		monitors->clear();
		for (auto *screen : QGuiApplication::screens()) {
			const auto size = screen->geometry().size();
			monitors->addItem(QStringLiteral("%1 · %2×%3").arg(screen->name()).arg(size.width()).arg(size.height()), screen->name());
		}
		int index = monitors->findData(previous);
		if (index >= 0) monitors->setCurrentIndex(index);
	}
	void enterFullscreen()
	{
		const auto screens = QGuiApplication::screens();
		int index = monitors->currentIndex();
		if (index < 0 || index >= screens.size()) return;
		normalGeometry = saveGeometry();
		showNormal();
		setGeometry(screens[index]->geometry());
		toolbar->hide();
		showFullScreen();
	}
	void leaveFullscreen()
	{
		showNormal();
		toolbar->show();
		if (!normalGeometry.isEmpty()) restoreGeometry(normalGeometry);
	}
	mv::LayoutConfig config;
	Canvas *canvas = nullptr;
	QToolBar *toolbar = nullptr;
	QComboBox *monitors = nullptr;
	QByteArray normalGeometry;
};

QPointer<MultiviewWindow> window;
QPointer<QAction> menuAction;
void frontendEvent(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		if (window) { window->shutdown(); delete window.data(); }
		if (menuAction) menuAction->setEnabled(false);
	}
}
} // namespace

bool obs_module_load(void)
{
	blog(LOG_INFO, "[multiview-plus] version 0.5.0 Controller loaded (OBS 32.2.2 / Windows x64)");
	return true;
}

void obs_module_post_load(void)
{
	menuAction = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction("Multiview Plus"));
	if (!menuAction) return;
	QObject::connect(menuAction, &QAction::triggered, menuAction, [] {
		if (!window) window = new MultiviewWindow;
		window->show();
		window->raise();
		window->activateWindow();
	});
	obs_frontend_add_event_callback(frontendEvent, nullptr);
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontendEvent, nullptr);
	if (window) delete window.data();
	if (menuAction) delete menuAction.data();
}
