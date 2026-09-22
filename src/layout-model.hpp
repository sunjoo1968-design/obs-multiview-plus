#pragma once
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace mv {
enum class TileKind { Program, Preview, Scene, Source, Stats, Empty, Clock, Resources };
struct TileConfig {
    TileKind kind = TileKind::Empty;
    QString uuid;
    QString label;
    QString tallyUuid;
    int x = 0, y = 0, w = 1, h = 1;
};
struct LayoutConfig {
    bool portrait = false;
    bool showNames = true;
    bool clickSwitch = false;
    bool doubleClickTransition = false;
    int columns = 4, rows = 4;
    QVector<TileConfig> tiles;
};
LayoutConfig defaultLayout();
LayoutConfig presetLayout(QString id, bool portrait);
QString validateLayout(const LayoutConfig &config);
bool swapLayoutTiles(LayoutConfig &config, int from, int to);
QJsonObject toJson(const LayoutConfig &config);
bool fromJson(const QJsonObject &object, LayoutConfig &config, QString *error = nullptr);
}
