#include "layout-model.hpp"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>
#include <limits>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int failures = 0;
    auto check = [&](bool pass, const char *name) { if (!pass) { qCritical() << "FAIL:" << name; ++failures; } };
    for (auto kind : {mv::TileKind::Clock, mv::TileKind::Resources}) {
        auto configured = mv::defaultLayout();
        configured.tiles[2].kind = kind;
        mv::LayoutConfig restored;
        check(mv::fromJson(mv::toJson(configured), restored) && restored.tiles[2].kind == kind, "information tile JSON roundtrip");
    }
    for (const QString id : {"sources4", "atem8", "grid4", "grid9", "grid16", "focus4", "focus8"}) {
        for (bool portrait : {false, true}) {
            auto c = mv::presetLayout(id, portrait);
            check(mv::validateLayout(c).isEmpty(), "preset valid");
            check(c.portrait == portrait, "preset aspect");
            if (id == "sources4") {
                for (const auto &tile : c.tiles) check(tile.kind == mv::TileKind::Empty, "source-only preset has no program/preview");
            } else if (id == "atem8") {
                check(c.tiles[0].kind == mv::TileKind::Preview && c.tiles[1].kind == mv::TileKind::Program, "ATEM preview left program right");
                check(c.columns == 4 && c.rows == 4 && c.tiles[0].w == 2 && c.tiles[0].h == 2 && c.tiles[1].x == 2, "ATEM upper geometry");
                for (int i = 2; i < 10; ++i) check(c.tiles[i].x == (i - 2) % 4 && c.tiles[i].y == 2 + (i - 2) / 4 && c.tiles[i].w == 1 && c.tiles[i].h == 1, "ATEM lower 4 by 2");
            } else check(c.tiles[0].kind == mv::TileKind::Program && c.tiles[1].kind == mv::TileKind::Preview, "program/preview included");
            const int expected = id == "grid4" || id == "sources4" ? 4 : id == "grid9" ? 9 : id == "grid16" ? 16 : id == "focus4" ? 6 : 10;
            check(c.tiles.size() == expected, "preset count");
            c.tiles[2].kind = mv::TileKind::Source; c.tiles[2].uuid = "source-uuid"; c.tiles[2].label = QStringLiteral("한글 카메라");
            c.tiles[2].tallyUuid = "camera-uuid";
            c.clickSwitch = true; c.doubleClickTransition = true; c.showNames = false;
            mv::LayoutConfig restored; QString error;
            check(mv::fromJson(mv::toJson(c), restored, &error), "roundtrip accepted");
            check(mv::toJson(restored) == mv::toJson(c), "roundtrip preserved");
        }
    }
    for (bool portrait : {false, true}) {
        for (bool reverse : {false, true}) {
            auto drag = mv::presetLayout("atem8", portrait);
            for (int i = 0; i < drag.tiles.size(); ++i) {
                drag.tiles[i].uuid = QString("uuid-%1").arg(i);
                drag.tiles[i].label = QString("label-%1").arg(i);
                drag.tiles[i].tallyUuid = QString("tally-%1").arg(i);
            }
            const auto before = drag;
            check(mv::swapLayoutTiles(drag, reverse ? 9 : 0, reverse ? 0 : 9), "ATEM halves drag both directions");
            for (int i = 0; i < drag.tiles.size(); ++i) {
                auto expected = before.tiles[i];
                expected.y += expected.y < 2 ? 2 : -2;
                auto actual = drag.tiles[i];
                check(actual.x == expected.x && actual.y == expected.y && actual.w == expected.w && actual.h == expected.h,
                      "ATEM half swap geometry");
                check(actual.uuid == expected.uuid && actual.label == expected.label && actual.tallyUuid == expected.tallyUuid && actual.kind == expected.kind,
                      "drag preserves tile metadata");
            }
            check(mv::validateLayout(drag).isEmpty(), "swapped ATEM valid");
            check(mv::swapLayoutTiles(drag, 1, 2), "ATEM halves swap back");
            check(mv::toJson(drag) == mv::toJson(before), "ATEM halves roundtrip");
            check(mv::swapLayoutTiles(drag, 0, 1), "large peers swap");
            check(drag.tiles[0].x == 2 && drag.tiles[1].x == 0, "large peers positions exchanged");
            check(mv::swapLayoutTiles(drag, 0, 1), "large peers swap back");
            check(mv::toJson(drag) == mv::toJson(before), "large peers preserve everything");
            check(mv::swapLayoutTiles(drag, 2, 9), "small peers swap");
            check(drag.tiles[2].x == 3 && drag.tiles[2].y == 3 && drag.tiles[9].x == 0 && drag.tiles[9].y == 2, "small peer positions");
            check(mv::swapLayoutTiles(drag, 2, 9), "small peers swap back");
            check(mv::toJson(drag) == mv::toJson(before), "small peers preserve everything");
        }
    }
    auto rejectsDrag = [&](mv::LayoutConfig drag, int from, int to) {
        const auto before = mv::toJson(drag);
        check(!mv::swapLayoutTiles(drag, from, to), "invalid drag rejected");
        check(mv::toJson(drag) == before, "invalid drag leaves layout unchanged");
    };
    rejectsDrag(mv::defaultLayout(), -1, 0);
    rejectsDrag(mv::defaultLayout(), 0, 10);
    rejectsDrag(mv::defaultLayout(), 1, 1);
    rejectsDrag(mv::presetLayout("focus4", false), 0, 2);
    auto malformed = mv::defaultLayout(); malformed.tiles[1].x = 0;
    rejectsDrag(malformed, 0, 2);
    malformed = mv::defaultLayout(); malformed.tiles[0].x = std::numeric_limits<int>::max();
    rejectsDrag(malformed, 0, 2);
    malformed = mv::defaultLayout(); malformed.tiles.removeLast();
    rejectsDrag(malformed, 0, 2);
    auto unfinished = mv::defaultLayout(); unfinished.tiles[2].kind = mv::TileKind::Scene;
    check(mv::swapLayoutTiles(unfinished, 0, 2), "editing allows unassigned scene metadata");
    check(!mv::validateLayout(unfinished).isEmpty(), "save still rejects unassigned scene");
    auto c = mv::defaultLayout();
    check(mv::toJson(c) == mv::toJson(mv::presetLayout("atem8", false)), "default is ATEM preset");
    check(!c.clickSwitch && !c.doubleClickTransition, "safe click defaults");
    c = mv::presetLayout("grid16", false);
    c.tiles.push_back(c.tiles.last()); check(!mv::validateLayout(c).isEmpty(), "17 rejected");
    c = mv::defaultLayout(); c.tiles.clear(); check(!mv::validateLayout(c).isEmpty(), "zero tiles rejected");
    c = mv::defaultLayout(); c.tiles[1].x = 0; check(!mv::validateLayout(c).isEmpty(), "overlap rejected");
    c = mv::defaultLayout(); c.tiles[0].x = -1; check(!mv::validateLayout(c).isEmpty(), "negative coordinate rejected");
    c = mv::defaultLayout(); c.tiles[0].w = 0; check(!mv::validateLayout(c).isEmpty(), "zero width rejected");
    c = mv::defaultLayout(); c.tiles[0].x = std::numeric_limits<int>::max(); check(!mv::validateLayout(c).isEmpty(), "overflow coordinate rejected");
    c = mv::defaultLayout(); c.tiles[0].kind = mv::TileKind::Scene; check(!mv::validateLayout(c).isEmpty(), "missing source rejected");
    c = mv::defaultLayout(); c.tiles[0].kind = static_cast<mv::TileKind>(99); check(!mv::validateLayout(c).isEmpty(), "invalid enum rejected");
    c = mv::defaultLayout(); c.tiles[0].label = QString(257, 'x'); check(!mv::validateLayout(c).isEmpty(), "excess label rejected");
    const auto valid = mv::toJson(mv::defaultLayout());
    auto invalid = [&](QJsonObject o, const char *name) {
        auto destination = mv::presetLayout("grid4", true); const auto before = mv::toJson(destination); QString error;
        check(!mv::fromJson(o, destination, &error), name);
        check(!error.isEmpty(), "failure explains reason");
        check(mv::toJson(destination) == before, "failed parse is transactional");
    };
    invalid({}, "empty object rejected");
    for (const char *key : {"version", "portrait", "showNames", "clickSwitch", "doubleClickTransition", "rows", "columns", "tiles"}) {
        auto o = valid; o.remove(key); invalid(o, "missing required field rejected");
    }
    auto o = valid; o["columns"] = 1.5; invalid(o, "fraction rejected");
    o = valid; o["rows"] = "4"; invalid(o, "string integer rejected");
    o = valid; o["portrait"] = 1; invalid(o, "numeric boolean rejected");
    o = valid; o["version"] = 3; invalid(o, "unknown version rejected");
    o = valid; o["tiles"] = QJsonArray{false}; invalid(o, "nonobject tile rejected");
    for (const QString field : {"kind", "uuid", "label", "tallyUuid", "x", "y", "w", "h"}) {
        o = valid; auto tiles = o["tiles"].toArray(); auto tile = tiles[0].toObject(); tile.remove(field); tiles[0] = tile; o["tiles"] = tiles;
        invalid(o, "missing tile field rejected");
    }
    o = valid; auto tiles = o["tiles"].toArray(); auto tile = tiles[0].toObject(); tile["kind"] = "bogus"; tiles[0] = tile; o["tiles"] = tiles; invalid(o, "unknown kind rejected");
    o = valid; tiles = o["tiles"].toArray(); tile = tiles[0].toObject(); tile["tallyUuid"] = false; tiles[0] = tile; o["tiles"] = tiles; invalid(o, "nonstring tally UUID rejected");
    o = valid; tiles = o["tiles"].toArray(); tile = tiles[0].toObject(); tile["tallyUuid"] = QString(257, 'x'); tiles[0] = tile; o["tiles"] = tiles; invalid(o, "overlong tally UUID rejected");
    o = valid; o["version"] = 1; tiles = o["tiles"].toArray();
    for (int i = 0; i < tiles.size(); ++i) { tile = tiles[i].toObject(); tile.remove("tallyUuid"); tiles[i] = tile; }
    o["tiles"] = tiles;
    mv::LayoutConfig legacy;
    check(mv::fromJson(o, legacy), "version one migration accepted");
    for (const auto &t : legacy.tiles) check(t.tallyUuid.isEmpty(), "legacy automatic tally");
    check(mv::toJson(legacy)["version"].toInt() == 2, "migrated save uses version two");
    qInfo() << (failures ? "Layout tests FAILED" : "All layout tests passed") << failures;
    return failures ? 1 : 0;
}
