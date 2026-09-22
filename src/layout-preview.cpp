#include "layout-preview.hpp"
#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
namespace mv {
LayoutPreview::LayoutPreview(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(260, 300); setMouseTracking(true);
    setObjectName("layoutPreview");
    setToolTip(QStringLiteral("칸을 다른 칸으로 드래그하면 위치를 교환합니다. 큰 화면을 작은 화면 영역으로 옮기면 두 영역 전체가 교환됩니다."));
}
void LayoutPreview::setLayoutConfig(const LayoutConfig &config) { config_ = config; update(); }
void LayoutPreview::setSelectedTile(int index) { selected_ = index; update(); }
QRect LayoutPreview::tileRect(int index) const
{
    if (index < 0 || index >= config_.tiles.size() || config_.columns < 1 || config_.rows < 1) return {};
    QSize size = config_.portrait ? QSize(9, 16) : QSize(16, 9);
    size.scale(qMax(1, width()-24), qMax(1, height()-24), Qt::KeepAspectRatio);
    const QPoint origin((width()-size.width())/2, (height()-size.height())/2);
    const auto &t = config_.tiles[index];
    const int x = t.x * size.width()/config_.columns, y = t.y*size.height()/config_.rows;
    const int right = (t.x+t.w)*size.width()/config_.columns, bottom = (t.y+t.h)*size.height()/config_.rows;
    return QRect(origin + QPoint(x,y), QSize(right-x,bottom-y)).adjusted(2,2,-2,-2);
}
int LayoutPreview::hitTest(QPoint point) const
{
    for (int i=0; i<config_.tiles.size(); ++i) if (tileRect(i).contains(point)) return i;
    return -1;
}
void LayoutPreview::paintEvent(QPaintEvent *)
{
    QPainter p(this); p.fillRect(rect(), QColor("#171a21"));
    for (int i=0; i<config_.tiles.size(); ++i) {
        const auto &t=config_.tiles[i]; const auto r=tileRect(i);
        const bool target=dragging_ && hovered_==i && i!=pressed_;
        p.fillRect(r, target ? QColor("#275d69") : (i==selected_ ? QColor("#33425d") : QColor("#272d38")));
        p.setPen(QPen(target ? QColor("#52d1dc") : (i==selected_ ? QColor("#8cb8ff") : QColor("#525d70")), i==selected_ || target ? 3 : 1)); p.drawRect(r);
        QString label=t.label;
        if (label.isEmpty()) {
            switch (t.kind) {
            case TileKind::Program: label=QStringLiteral("프로그램"); break;
            case TileKind::Preview: label=QStringLiteral("프리뷰"); break;
            case TileKind::Scene: label=QStringLiteral("장면"); break;
            case TileKind::Source: label=QStringLiteral("소스"); break;
            case TileKind::Stats: label=QStringLiteral("통계"); break;
            case TileKind::Clock: label=QStringLiteral("시계"); break;
            case TileKind::Resources: label=QStringLiteral("CPU / GPU (OBS)"); break;
            default: label=QStringLiteral("빈칸"); break;
            }
        }
        p.setPen(Qt::white);
        const QString text=QString::number(i+1)+QStringLiteral("\n")+p.fontMetrics().elidedText(label, Qt::ElideRight, qMax(1,r.width()-12));
        p.drawText(r.adjusted(5,5,-5,-5), Qt::AlignCenter, text);
    }
}
void LayoutPreview::mousePressEvent(QMouseEvent *event)
{
    if (event->button()!=Qt::LeftButton) return;
    pressed_=hitTest(event->position().toPoint()); pressPosition_=event->position().toPoint();
    if (pressed_>=0) { selected_=pressed_; emit tileSelected(pressed_); update(); }
}
void LayoutPreview::mouseMoveEvent(QMouseEvent *event)
{
    hovered_=hitTest(event->position().toPoint());
    if (pressed_>=0 && (event->buttons() & Qt::LeftButton) && (event->position().toPoint()-pressPosition_).manhattanLength()>=QApplication::startDragDistance()) {
        dragging_=true; setCursor(Qt::ClosedHandCursor);
    }
    update();
}
void LayoutPreview::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button()!=Qt::LeftButton) return;
    const int from=pressed_, to=hitTest(event->position().toPoint());
    const bool drop=dragging_ && from>=0 && to>=0 && from!=to;
    pressed_=-1; hovered_=-1; dragging_=false; unsetCursor();
    if (drop) emit tilesDropped(from,to);
    update();
}
}
