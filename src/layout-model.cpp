#include "layout-model.hpp"
#include <QJsonArray>
#include <cmath>
#include <utility>

namespace mv {
namespace {
QString kindName(TileKind kind)
{
    switch (kind) {
    case TileKind::Program: return "program";
    case TileKind::Preview: return "preview";
    case TileKind::Scene: return "scene";
    case TileKind::Source: return "source";
    case TileKind::Stats: return "stats";
    case TileKind::Empty: return "empty";
    case TileKind::Clock: return "clock";
    case TileKind::Resources: return "resources";
    }
    return {};
}
bool readInt(const QJsonObject &o, const char *key, int &out)
{
    const auto v = o.value(QLatin1String(key));
    if (!v.isDouble()) return false;
    const double n = v.toDouble();
    if (!std::isfinite(n) || n != std::floor(n) || n < -100000 || n > 100000) return false;
    out = static_cast<int>(n);
    return true;
}
}
LayoutConfig presetLayout(QString id, bool portrait)
{
    LayoutConfig c;
    c.portrait = portrait;
    if (id == "atem8") {
        c.columns = c.rows = 4;
        for (int i = 0; i < 2; ++i) {
            TileConfig t; t.kind = i == 0 ? TileKind::Preview : TileKind::Program;
            t.x = i * 2; t.w = t.h = 2; c.tiles.push_back(t);
        }
        for (int y = 2; y < 4; ++y) for (int x = 0; x < 4; ++x) {
            TileConfig t; t.x = x; t.y = y; c.tiles.push_back(t);
        }
    } else if (id == "focus4" || id == "focus8") {
        c.columns = 4;
        const int lowerRows = id == "focus4" ? 2 : 4;
        c.rows = lowerRows + 2;
        for (int i = 0; i < 2; ++i) {
            TileConfig t;
            t.kind = i == 0 ? TileKind::Program : TileKind::Preview;
            t.x = i * 2; t.w = 2; t.h = 2;
            c.tiles.push_back(t);
        }
        for (int y = 0; y < lowerRows; ++y)
            for (int x = 0; x < 2; ++x) {
                TileConfig t; t.x = x * 2; t.y = y + 2; t.w = 2;
                c.tiles.push_back(t);
            }
    } else {
        const int n = id == "grid4" || id == "sources4" ? 2 : id == "grid9" ? 3 : 4;
        c.columns = c.rows = n;
        for (int i = 0; i < n * n; ++i) {
            TileConfig t; t.x = i % n; t.y = i / n;
            if (id != "sources4" && i == 0) t.kind = TileKind::Program;
            if (id != "sources4" && i == 1) t.kind = TileKind::Preview;
            c.tiles.push_back(t);
        }
    }
    return c;
}
LayoutConfig defaultLayout() { return presetLayout("atem8", false); }
static QString validateGeometry(const LayoutConfig &c)
{
    if (c.columns < 1 || c.columns > 16 || c.rows < 1 || c.rows > 16)
        return QStringLiteral("격자 행과 열은 1~16이어야 합니다.");
    if (c.tiles.isEmpty() || c.tiles.size() > 16)
        return QStringLiteral("칸은 1~16개여야 합니다. 프로그램·프리뷰·통계·빈칸도 개수에 포함됩니다.");
    for (int i = 0; i < c.tiles.size(); ++i) {
        const auto &t = c.tiles[i];
        if (t.x < 0 || t.y < 0 || t.w < 1 || t.h < 1 || t.w > c.columns || t.h > c.rows ||
            t.x > c.columns - t.w || t.y > c.rows - t.h)
            return QStringLiteral("%1번 칸이 격자 경계를 벗어납니다.").arg(i + 1);
        for (int j = 0; j < i; ++j) {
            const auto &u = c.tiles[j];
            if (t.x < u.x + u.w && u.x < t.x + t.w && t.y < u.y + u.h && u.y < t.y + t.h)
                return QStringLiteral("%1번 칸과 %2번 칸이 겹칩니다.").arg(j + 1).arg(i + 1);
        }
    }
    return {};
}
QString validateLayout(const LayoutConfig &c)
{
    const auto geometryIssue = validateGeometry(c);
    if (!geometryIssue.isEmpty()) return geometryIssue;
    for (int i = 0; i < c.tiles.size(); ++i) {
        const auto &t = c.tiles[i];
        if (kindName(t.kind).isEmpty()) return QStringLiteral("알 수 없는 칸 종류입니다.");
        if ((t.kind == TileKind::Scene || t.kind == TileKind::Source) && t.uuid.trimmed().isEmpty())
            return QStringLiteral("%1번 칸의 장면 또는 소스를 선택하세요.").arg(i + 1);
        if (t.uuid.size() > 256 || t.label.size() > 256 || t.tallyUuid.size() > 256)
            return QStringLiteral("UUID와 이름은 256자 이하여야 합니다.");
    }
    return {};
}
bool swapLayoutTiles(LayoutConfig &c, int from, int to)
{
    if (from < 0 || to < 0 || from >= c.tiles.size() || to >= c.tiles.size() || from == to ||
        !validateGeometry(c).isEmpty()) return false;
    LayoutConfig candidate = c;
    auto &a = candidate.tiles[from];
    auto &b = candidate.tiles[to];
    if (a.w == b.w && a.h == b.h) {
        std::swap(a.x, b.x);
        std::swap(a.y, b.y);
    } else {
        // The ATEM layout moves as two complete halves, not overlapping individual tiles.
        if (c.columns != 4 || c.rows != 4 || c.tiles.size() != 10) return false;
        int largeY = -1, largeCount = 0, smallCount = 0;
        for (const auto &t : c.tiles) {
            if (t.w == 2 && t.h == 2 && (t.x == 0 || t.x == 2) && (t.y == 0 || t.y == 2)) {
                if (largeY != -1 && largeY != t.y) return false;
                largeY = t.y;
                ++largeCount;
            } else if (t.w == 1 && t.h == 1) {
                ++smallCount;
            } else return false;
        }
        if (largeCount != 2 || smallCount != 8) return false;
        for (auto &t : candidate.tiles) {
            if (t.w == 1 && (t.y / 2) == (largeY / 2)) return false;
            t.y += t.y < 2 ? 2 : -2;
        }
    }
    if (!validateGeometry(candidate).isEmpty()) return false;
    c = std::move(candidate);
    return true;
}
QJsonObject toJson(const LayoutConfig &c)
{
    QJsonArray tiles;
    for (const auto &t : c.tiles)
        tiles.append(QJsonObject{{"kind", kindName(t.kind)}, {"uuid", t.uuid}, {"label", t.label}, {"tallyUuid", t.tallyUuid},
                                 {"x", t.x}, {"y", t.y}, {"w", t.w}, {"h", t.h}});
    return {{"version", 2}, {"portrait", c.portrait}, {"showNames", c.showNames},
            {"clickSwitch", c.clickSwitch}, {"doubleClickTransition", c.doubleClickTransition},
            {"columns", c.columns}, {"rows", c.rows}, {"tiles", tiles}};
}
bool fromJson(const QJsonObject &o, LayoutConfig &config, QString *error)
{
    auto fail = [&](const QString &message) { if (error) *error = message; return false; };
    int version;
    if (!readInt(o, "version", version) || (version != 1 && version != 2)) return fail(QStringLiteral("지원하지 않는 설정 버전입니다."));
    LayoutConfig c;
    if (!readInt(o, "columns", c.columns) || !readInt(o, "rows", c.rows)) return fail(QStringLiteral("격자 크기가 정수가 아닙니다."));
    for (const char *key : {"portrait", "showNames", "clickSwitch", "doubleClickTransition"})
        if (!o.value(QLatin1String(key)).isBool()) return fail(QStringLiteral("설정의 논리값 형식이 잘못되었습니다."));
    c.portrait = o["portrait"].toBool(); c.showNames = o["showNames"].toBool();
    c.clickSwitch = o["clickSwitch"].toBool(); c.doubleClickTransition = o["doubleClickTransition"].toBool();
    if (!o["tiles"].isArray()) return fail(QStringLiteral("칸 목록이 없습니다."));
    const auto array = o["tiles"].toArray();
    if (array.isEmpty() || array.size() > 16) return fail(QStringLiteral("칸 개수가 허용 범위를 벗어났습니다."));
    for (const auto &v : array) {
        if (!v.isObject()) return fail(QStringLiteral("칸 설정 형식이 잘못되었습니다."));
        const auto t = v.toObject(); TileConfig tile;
        if (!t["kind"].isString() || !t["uuid"].isString() || !t["label"].isString())
            return fail(QStringLiteral("칸 종류, UUID, 이름은 문자열이어야 합니다."));
        bool found = false;
        for (auto kind : {TileKind::Program, TileKind::Preview, TileKind::Scene, TileKind::Source, TileKind::Stats, TileKind::Empty, TileKind::Clock, TileKind::Resources})
            if (kindName(kind) == t["kind"].toString()) { tile.kind = kind; found = true; break; }
        if (!found) return fail(QStringLiteral("알 수 없는 칸 종류입니다."));
        tile.uuid = t["uuid"].toString(); tile.label = t["label"].toString();
        if ((version == 2 || t.contains("tallyUuid")) && !t["tallyUuid"].isString())
            return fail(QStringLiteral("탈리 기준 UUID는 문자열이어야 합니다."));
        tile.tallyUuid = t["tallyUuid"].toString();
        if (!readInt(t, "x", tile.x) || !readInt(t, "y", tile.y) || !readInt(t, "w", tile.w) || !readInt(t, "h", tile.h))
            return fail(QStringLiteral("칸 좌표와 크기는 정수여야 합니다."));
        c.tiles.push_back(tile);
    }
    const auto issue = validateLayout(c);
    if (!issue.isEmpty()) return fail(issue);
    config = c;
    if (error) error->clear();
    return true;
}
}
