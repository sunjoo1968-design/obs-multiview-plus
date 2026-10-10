#!/bin/bash
#
# Baut das Sunjoo OBS Link Multiview Plugin fuer macOS (Intel und Apple Silicon).
#
# Das Skript
#   1. liest die Version der installierten OBS-App,
#   2. laedt die dazu passenden OBS-Header und das passende Qt (mit Pruefsumme),
#   3. baut das Plugin und die Unit-Tests,
#   4. erzeugt ein .plugin-Bundle, signiert es lokal (ad hoc) und packt es als ZIP,
#   5. installiert es auf Wunsch fuer OBS.
#
# Ergebnis: dist/macos/obs-multiview-plus.plugin und ein ZIP daneben.
# Mehr dazu: docs/MACOS.md

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLUGIN="obs-multiview-plus"
DEPS="$ROOT/.deps"
DIST="$ROOT/dist/macos"
PLISTBUDDY="${PLISTBUDDY:-/usr/libexec/PlistBuddy}"

OBS_APP="/Applications/OBS.app"
OBS_TAG=""
UNIVERSAL=0
INSTALL=0
RUN_TESTS=1
CLEAN=0

# simde: SIMD-Kompatibilitaetsschicht, die die libobs-Header auf macOS brauchen.
SIMDE_TAG="v0.8.2"
SIMDE_COMMIT="71fd833d9666141edcd1d3c109a80e228303d8d7"

usage() {
    cat <<'EOF'
Verwendung: scripts/build-macos.sh [Optionen]

  --obs-app PFAD   Pfad zur OBS.app (Standard: /Applications/OBS.app)
  --obs-tag TAG    OBS-Version fuer die Header, z. B. 33.0.0-rc1
                   (Standard: Version der installierten OBS-App)
  --universal      Eine Datei fuer Intel und Apple Silicon (experimentell)
  --install        Plugin nach ~/Library/Application Support/obs-studio/plugins kopieren
  --skip-tests     Unit-Tests ueberspringen
  --clean          Build-Ordner vorher loeschen
  -h, --help       Diese Hilfe
EOF
}

info() { printf '\n==> %s\n' "$*"; }
warn() { printf 'WARNUNG: %s\n' "$*" >&2; }
die()  { printf 'FEHLER: %s\n' "$*" >&2; exit 1; }
need() { command -v "$1" >/dev/null 2>&1 || die "'$1' fehlt. $2"; }

while [ $# -gt 0 ]; do
    case "$1" in
        --obs-app)
            [ $# -ge 2 ] || die "--obs-app braucht einen Pfad."
            OBS_APP="$2"; shift 2 ;;
        --obs-tag)
            [ $# -ge 2 ] || die "--obs-tag braucht eine Version."
            OBS_TAG="$2"; shift 2 ;;
        --universal)  UNIVERSAL=1; shift ;;
        --install)    INSTALL=1; shift ;;
        --skip-tests) RUN_TESTS=0; shift ;;
        --clean)      CLEAN=1; shift ;;
        -h|--help)    usage; exit 0 ;;
        *)            die "Unbekannte Option: $1 (siehe --help)" ;;
    esac
done

# ---------------------------------------------------------------- Voraussetzungen
[ "$(uname -s)" = "Darwin" ] || die "Dieses Skript laeuft nur auf macOS."
xcode-select -p >/dev/null 2>&1 || die "Die Xcode Command Line Tools fehlen. Installieren mit: xcode-select --install"
need cmake   "Installieren mit: brew install cmake (oder von cmake.org)."
need git     "Kommt mit den Xcode Command Line Tools."
need curl    "Gehoert zu macOS."
need python3 "Kommt mit den Xcode Command Line Tools."
for tool in shasum tar lipo otool codesign plutil ditto xattr nm; do
    need "$tool" "Gehoert zu macOS bzw. den Command Line Tools."
done
[ -x "$PLISTBUDDY" ] || die "PlistBuddy nicht gefunden ($PLISTBUDDY)."

CMAKE_OK="$(cmake --version | head -n1 | awk '{split($3, v, "."); print (v[1] > 3 || (v[1] == 3 && v[2] >= 28)) ? "yes" : "no"}')"
[ "$CMAKE_OK" = "yes" ] || die "CMake 3.28 oder neuer wird gebraucht (gefunden: $(cmake --version | head -n1))."

VERSION="$(sed -n 's/^project(obs-multiview-plus VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
[ -n "$VERSION" ] || die "Plugin-Version in CMakeLists.txt nicht gefunden."

# ---------------------------------------------------------------- Installierte OBS-App
[ -d "$OBS_APP" ] || die "OBS.app nicht gefunden: $OBS_APP (Pfad mit --obs-app angeben)."
OBS_VERSION="$("$PLISTBUDDY" -c 'Print :CFBundleShortVersionString' "$OBS_APP/Contents/Info.plist" 2>/dev/null || true)"
OBS_ARCHS="$(lipo -archs "$OBS_APP/Contents/MacOS/OBS" 2>/dev/null || true)"
info "Installiertes OBS: ${OBS_VERSION:-unbekannt} (${OBS_ARCHS:-Architektur unbekannt})"

if [ -z "$OBS_TAG" ]; then
    [ -n "$OBS_VERSION" ] || die "OBS-Version nicht lesbar. Bitte mit --obs-tag angeben."
    OBS_TAG="$OBS_VERSION"
fi
if ! git ls-remote --exit-code --tags https://github.com/obsproject/obs-studio "refs/tags/$OBS_TAG" >/dev/null 2>&1; then
    warn "Es gibt kein OBS-Release mit der Bezeichnung '$OBS_TAG'. Zuletzt veroeffentlicht:"
    git ls-remote --tags --refs https://github.com/obsproject/obs-studio 'refs/tags/3*' 2>/dev/null \
        | sed 's#.*refs/tags/##' | tail -n 12 >&2 || true
    die "Starte das Skript mit --obs-tag <Version> erneut (die zu deinem OBS passende Version)."
fi

# ---------------------------------------------------------------- Abhaengigkeiten
mkdir -p "$DEPS"

fetch_sparse() { # url ref zielordner ordner...
    local url="$1" ref="$2" dir="$3"
    shift 3
    rm -rf "$dir"
    git -c advice.detachedHead=false clone --quiet --depth 1 --filter=blob:none --sparse --branch "$ref" "$url" "$dir"
    git -C "$dir" sparse-checkout set "$@"
}

if [ "$(cat "$DEPS/obs-studio/.mv-ref" 2>/dev/null || true)" != "$OBS_TAG" ] || [ ! -f "$DEPS/obs-studio/libobs/obs.h" ]; then
    info "Lade OBS-Header ($OBS_TAG)"
    fetch_sparse https://github.com/obsproject/obs-studio "$OBS_TAG" "$DEPS/obs-studio" libobs frontend/api
    printf '%s' "$OBS_TAG" > "$DEPS/obs-studio/.mv-ref"
fi

if [ ! -f "$DEPS/simde/simde/x86/sse2.h" ] || [ "$(git -C "$DEPS/simde" rev-parse HEAD 2>/dev/null || true)" != "$SIMDE_COMMIT" ]; then
    info "Lade simde ($SIMDE_TAG)"
    fetch_sparse https://github.com/simd-everywhere/simde "$SIMDE_TAG" "$DEPS/simde" simde
    [ "$(git -C "$DEPS/simde" rev-parse HEAD)" = "$SIMDE_COMMIT" ] \
        || die "simde: unerwarteter Commit. Aus Sicherheitsgruenden abgebrochen."
fi

# Das passende Qt steht in den Build-Vorgaben der jeweiligen OBS-Version.
QT_INFO="$(python3 - "$DEPS/obs-studio/CMakePresets.json" <<'PY'
import json
import sys

data = json.load(open(sys.argv[1]))
for preset in data.get("configurePresets", []):
    deps = preset.get("vendor", {}).get("obsproject.com/obs-studio", {}).get("dependencies", {})
    qt = deps.get("qt6")
    if qt:
        print("|".join([qt["version"], qt["baseUrl"], qt["hashes"]["macos-universal"]]))
        break
else:
    sys.exit("Qt-Eintrag in CMakePresets.json nicht gefunden")
PY
)"
QT_VERSION="${QT_INFO%%|*}"
QT_REST="${QT_INFO#*|}"
QT_BASE="${QT_REST%%|*}"
QT_SHA="${QT_REST#*|}"
QT_ARCHIVE="$DEPS/qt6-$QT_VERSION.tar.xz"
QT_URL="$QT_BASE/$QT_VERSION/macos-deps-qt6-$QT_VERSION-universal.tar.xz"

if [ ! -f "$QT_ARCHIVE" ]; then
    info "Lade Qt-Paket $QT_VERSION"
    curl -fL --retry 3 --progress-bar -o "$QT_ARCHIVE.partial" "$QT_URL"
    mv "$QT_ARCHIVE.partial" "$QT_ARCHIVE"
fi
QT_ACTUAL="$(shasum -a 256 "$QT_ARCHIVE" | awk '{print $1}')"
if [ "$QT_ACTUAL" != "$QT_SHA" ]; then
    rm -f "$QT_ARCHIVE"
    die "Pruefsumme des Qt-Pakets stimmt nicht. Die Datei wurde geloescht, bitte erneut starten."
fi
if [ "$(cat "$DEPS/qt6/.mv-version" 2>/dev/null || true)" != "$QT_VERSION" ]; then
    info "Entpacke Qt"
    rm -rf "$DEPS/qt6"
    mkdir -p "$DEPS/qt6"
    tar -xf "$QT_ARCHIVE" -C "$DEPS/qt6"
    printf '%s' "$QT_VERSION" > "$DEPS/qt6/.mv-version"
fi
[ -f "$DEPS/qt6/lib/cmake/Qt6/Qt6Config.cmake" ] || die "Das Qt-Paket hat einen unerwarteten Aufbau."

QT_BUILT="$(sed -n 's/^set(PACKAGE_VERSION "\(.*\)")/\1/p' "$DEPS/qt6/lib/cmake/Qt6/Qt6ConfigVersionImpl.cmake" 2>/dev/null | head -n1 || true)"
QT_IN_OBS="$("$PLISTBUDDY" -c 'Print :CFBundleShortVersionString' \
    "$OBS_APP/Contents/Frameworks/QtCore.framework/Versions/A/Resources/Info.plist" 2>/dev/null || true)"
info "Qt: gebaut gegen ${QT_BUILT:-?}, in OBS enthalten: ${QT_IN_OBS:-unbekannt}"
if [ -n "$QT_BUILT" ] && [ -n "$QT_IN_OBS" ] && [ "${QT_BUILT%.*}" != "${QT_IN_OBS%.*}" ]; then
    warn "Die Qt-Version in OBS ($QT_IN_OBS) weicht von der Build-Version ($QT_BUILT) ab. Das Plugin kann dann beim Start abstuerzen. Passende --obs-tag-Version pruefen."
fi

# ---------------------------------------------------------------- Bauen
if [ "$UNIVERSAL" = 1 ]; then
    BUILD="$ROOT/build/macos-universal"
    ARCH_ARG="-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64"
else
    BUILD="$ROOT/build/macos-native"
    case "$OBS_ARCHS" in
        arm64|x86_64) ARCH_ARG="-DCMAKE_OSX_ARCHITECTURES=$OBS_ARCHS" ;;
        *)            ARCH_ARG="" ;;
    esac
fi
if [ "$CLEAN" = 1 ]; then rm -rf "$BUILD"; fi

info "Konfiguriere"
cmake -S "$ROOT" -B "$BUILD" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
    "-DOBS_SOURCE_DIR=$DEPS/obs-studio" "-DOBS_SIMDE_DIR=$DEPS/simde" "-DOBS_APP=$OBS_APP" \
    ${ARCH_ARG:+"$ARCH_ARG"}

info "Baue"
cmake --build "$BUILD" --parallel "$(sysctl -n hw.ncpu)"

if [ "$RUN_TESTS" = 1 ]; then
    info "Fuehre Unit-Tests aus"
    if ! ctest --test-dir "$BUILD" --output-on-failure; then
        warn "Unit-Tests fehlgeschlagen. Das Plugin wird trotzdem gepackt; bitte die Ausgabe oben aufbewahren."
    fi
fi

# ---------------------------------------------------------------- Bundle
BUNDLE="$DIST/$PLUGIN.plugin"
SO="$BUILD/$PLUGIN.so"
[ -f "$SO" ] || die "Gebaute Datei nicht gefunden: $SO"

info "Erzeuge $PLUGIN.plugin"
rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/Contents/MacOS" "$BUNDLE/Contents/Resources"
cp "$SO" "$BUNDLE/Contents/MacOS/$PLUGIN"
cp "$ROOT/README.md" "$ROOT/LICENSE" "$BUNDLE/Contents/Resources/"
cat > "$BUNDLE/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key><string>en</string>
    <key>CFBundleExecutable</key><string>$PLUGIN</string>
    <key>CFBundleIdentifier</key><string>com.github.$PLUGIN</string>
    <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
    <key>CFBundleName</key><string>$PLUGIN</string>
    <key>CFBundlePackageType</key><string>BNDL</string>
    <key>CFBundleShortVersionString</key><string>$VERSION</string>
    <key>CFBundleVersion</key><string>$VERSION</string>
    <key>LSMinimumSystemVersion</key><string>12.0</string>
</dict>
</plist>
EOF
plutil -lint "$BUNDLE/Contents/Info.plist" >/dev/null

BIN="$BUNDLE/Contents/MacOS/$PLUGIN"
ARCHS="$(lipo -archs "$BIN")"

# Das Plugin darf weder auf Bibliotheken des Build-Ordners zeigen noch eine eigene
# Qt-Kopie laden: zwei Qt-Versionen im selben Prozess fuehren zu Abstuerzen.
# Nur die eingerueckten Abhaengigkeitszeilen pruefen, nicht die Kopfzeile mit dem eigenen Pfad.
if otool -L "$BIN" | grep -E '^[[:space:]]' | grep -E '\.deps|/Users/|/private/|/Volumes/' >/dev/null; then
    otool -L "$BIN" >&2
    die "Das Plugin verweist auf Bibliotheken ausserhalb von OBS (siehe oben)."
fi
BAD_RPATH="$(otool -l "$BIN" | awk '/cmd LC_RPATH/ {getline; getline; print $2}' | grep -v '^@' || true)"
[ -z "$BAD_RPATH" ] || die "Das Plugin enthaelt einen festen Suchpfad: $BAD_RPATH"
if [ "$(nm -gU "$BIN" 2>/dev/null | grep -cE '[[:space:]]_obs_module_(ver|set_pointer)$' || true)" -lt 2 ]; then
    warn "Die OBS-Modul-Symbole wurden nicht gefunden. OBS wuerde das Plugin dann nicht laden."
fi

info "Signiere lokal (ad hoc)"
codesign --force --sign - "$BUNDLE"
codesign --verify --strict "$BUNDLE"

ZIP="$DIST/$PLUGIN-$VERSION-macos-$(printf '%s' "$ARCHS" | tr ' ' '+').zip"
rm -f "$ZIP"
ditto -c -k --keepParent "$BUNDLE" "$ZIP"
( cd "$DIST" && shasum -a 256 "$(basename "$ZIP")" > "$(basename "$ZIP").sha256" )

# ---------------------------------------------------------------- Installation
if [ "$INSTALL" = 1 ]; then
    if pgrep -x OBS >/dev/null 2>&1; then
        die "OBS laeuft noch. Bitte OBS beenden und das Skript mit --install erneut starten (das Ergebnis liegt schon in $DIST)."
    fi
    TARGET="$HOME/Library/Application Support/obs-studio/plugins"
    info "Installiere nach $TARGET"
    mkdir -p "$TARGET"
    rm -rf "$TARGET/$PLUGIN.plugin"
    ditto "$BUNDLE" "$TARGET/$PLUGIN.plugin"
    xattr -dr com.apple.quarantine "$TARGET/$PLUGIN.plugin" 2>/dev/null || true
    if [ -d "$TARGET/$PLUGIN" ]; then
        warn "Zusaetzlich existiert ein alter Plugin-Ordner: $TARGET/$PLUGIN. Bitte entfernen, damit das Plugin nicht doppelt geladen wird."
    fi
fi

cat <<EOF

Fertig.
  Plugin:        $BUNDLE
  Architektur:   $ARCHS
  ZIP zum Weitergeben: $ZIP
EOF
if [ "$INSTALL" = 1 ]; then
    echo "  Installiert:   OBS starten, dann Menue Werkzeuge -> Sunjoo OBS Link Multiview"
else
    echo "  Installieren:  scripts/build-macos.sh --install   (OBS vorher beenden)"
fi
echo "  OBS-Logs bei Problemen: ~/Library/Application Support/obs-studio/logs"
