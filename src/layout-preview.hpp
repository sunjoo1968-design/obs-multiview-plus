#pragma once
#include "layout-model.hpp"
#include <QWidget>
namespace mv {
class LayoutPreview : public QWidget {
    Q_OBJECT
public:
    explicit LayoutPreview(QWidget *parent = nullptr);
    void setLayoutConfig(const LayoutConfig &config);
    void setSelectedTile(int index);
    Q_INVOKABLE QRect tileRect(int index) const;
signals:
    void tileSelected(int index);
    void tilesDropped(int from, int to);
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
private:
    int hitTest(QPoint point) const;
    LayoutConfig config_;
    int selected_ = -1, pressed_ = -1, hovered_ = -1;
    QPoint pressPosition_;
    bool dragging_ = false;
};
}
