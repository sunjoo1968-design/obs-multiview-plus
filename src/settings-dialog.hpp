#pragma once
#include "layout-model.hpp"
#include <QDialog>
class QCheckBox;
class QComboBox;
class QTableWidget;
namespace mv {
class LayoutPreview;
class SettingsDialog : public QDialog {
public:
    SettingsDialog(QWidget *parent, const LayoutConfig &config);
    LayoutConfig resultConfig() const;
private:
    void load(const LayoutConfig &config);
    void appendTile(const TileConfig &tile);
    void populateSources(int row, const QString &uuid);
    void updatePreview();
    LayoutConfig geometry_;
    bool loading_ = false;
    LayoutPreview *preview_;
    QComboBox *ratio_;
    QComboBox *preset_;
    QCheckBox *names_;
    QCheckBox *click_;
    QCheckBox *doubleClick_;
    QTableWidget *table_;
};
}
