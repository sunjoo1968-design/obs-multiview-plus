# Sunjoo OBS Link Multiview auf macOS (experimentell)

Unterstützt werden Intel- und Apple-Silicon-Macs mit OBS 33.x. Der Code wurde gegen die Header von
OBS 33.0.0-rc1 übersetzt und die plattformneutralen Tests laufen durch. **Auf einem Mac selbst wurde er
noch nicht gestartet.** Bitte beim ersten Test genau hinschauen und Auffälligkeiten melden.

## Voraussetzungen

- macOS 12 oder neuer, OBS 33.x in `/Applications/OBS.app`
- Xcode Command Line Tools: `xcode-select --install`
- CMake 3.28 oder neuer: `brew install cmake` oder der Installer von cmake.org
- Internet beim ersten Bauen (OBS-Header, Qt, simde werden geladen und per Prüfsumme bzw. Commit geprüft)

## Bauen und installieren

OBS beenden, dann im Terminal im Projektordner:

```bash
bash scripts/build-macos.sh --install
```

Das Skript erkennt die Version und Architektur der installierten OBS-App, lädt dazu passende Header und
das passende Qt, baut das Plugin und installiert es nach
`~/Library/Application Support/obs-studio/plugins/obs-multiview-plus.plugin`.

- **Jeder Mac baut sich sein Plugin selbst.** Das dauert wenige Minuten und passt automatisch zur
  Architektur der dortigen OBS-App (Intel oder Apple Silicon).
- Ohne `--install` wird nur gebaut. Ergebnis: `dist/macos/obs-multiview-plus.plugin` und eine ZIP-Datei
  daneben, die auf einen Mac **derselben Architektur** kopiert werden kann. Nach dem Kopieren einmal
  `xattr -dr com.apple.quarantine obs-multiview-plus.plugin` ausführen.
- `--universal` baut eine Datei für beide Architekturen. Das ist experimentell; im Zweifel
  auf jedem Mac normal bauen.
- Läuft OBS als Beta oder Release Candidate, das Skript aber findet die Version nicht, hilft
  `--obs-tag 33.0.0-rc1` (die zu deinem OBS passende Version).
- Weitere Optionen: `bash scripts/build-macos.sh --help`

Starten: OBS öffnen, Menü **Werkzeuge → Sunjoo OBS Link Multiview**.

Entfernen: Ordner `obs-multiview-plus.plugin` aus
`~/Library/Application Support/obs-studio/plugins` löschen.

## Unterschiede zu Windows

- **CPU/GPU-Kachel:** Die CPU-Auslastung des OBS-Prozesses wird angezeigt. Für die GPU gibt es unter macOS
  keine Messung pro Prozess; die Kachel zeigt dort „nicht messbar“ statt eines falschen Wertes.
- **Vollbild:** Es wird der native macOS-Vollbildmodus auf dem gewählten Monitor verwendet. Esc beendet ihn.
  Erscheint das Vollbild nicht auf dem gewählten Monitor, bitte melden, dann wird dieser Teil angepasst.
- **Einstellungen übernehmen:** Die Oberfläche ist koreanisch beschriftet. Auf dem Windows-PC in der
  Werkzeugleiste **프리셋 내보내기** (Preset exportieren) wählen und die JSON-Datei speichern, am Mac
  **프리셋 가져오기** (Preset importieren) und diese Datei laden. Die Einstellungen liegen am Mac in
  `~/Library/Application Support/obs-studio/plugin_config/obs-multiview-plus/layout.json`.
- **Weitere Knöpfe:** **레이아웃 설정** = Layout-Einstellungen, **전체 화면** = Vollbild.

## Wenn etwas nicht funktioniert

1. Die letzte Log-Datei in `~/Library/Application Support/obs-studio/logs` öffnen und nach `multiview` suchen.
   Beim erfolgreichen Laden steht dort `[multiview-plus] ... loaded (macOS)`.
2. Für eine Rückmeldung bitte mitschicken: die komplette Terminal-Ausgabe des Skripts oder die Log-Zeilen,
   die Ausgabe von `sw_vers` und von `lipo -archs /Applications/OBS.app/Contents/MacOS/OBS`.
3. Meldet macOS beim Start, die Datei sei nicht verifiziert, nach der Installation
   `xattr -dr com.apple.quarantine ~/Library/Application\ Support/obs-studio/plugins/obs-multiview-plus.plugin`
   ausführen. Das Skript tut das bei `--install` selbst.

## Technische Hinweise

- Auf macOS hängt das Plugin an der installierten OBS-App (`libobs` und `obs-frontend-api`) und nutzt deren Qt.
  Das Skript prüft, dass kein fester Pfad zu einem anderen Qt im Plugin steckt, und warnt, wenn die Qt-Version
  der Build-Umgebung von der in OBS abweicht.
- Das Plugin wird lokal ad hoc signiert. Eine Apple-Entwickler-Signatur oder Notarisierung ist für den
  eigenen Gebrauch nicht nötig.
- Die Windows-Laufzeittests (`scripts/test-runtime.ps1`, Smoke-Driver) sind nicht portiert. Die
  Unit-Tests (`layout-test`, `tally-test`) laufen auch am Mac und werden vom Skript ausgeführt.
- Nach einem OBS-Update mit neuer Qt-Version das Plugin neu bauen.
