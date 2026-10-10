# Sunjoo OBS Link Multiview – macOS-Fork

Multiview-Plugin für OBS Studio mit Tally-Rahmen, frei anordenbaren Kacheln und Vollbild auf einem
wählbaren Monitor.

**Dies ist ein Fork für macOS (Intel und Apple Silicon, OBS 33) mit deutscher Oberfläche.**
Das Original von **SunjooAn** ist ein reines Windows-Plugin mit koreanischer Oberfläche:
[sunjoo1968-design/obs-multiview-plus](https://github.com/sunjoo1968-design/obs-multiview-plus).
Funktionsumfang und Tally-Logik stammen aus dem Original (Version 0.5.4). Dieser Fork ergänzt:

- macOS-Unterstützung (Bauskript, Plugin-Bundle, Vollbild, Darstellung der Videokacheln)
- deutsche Oberfläche mit den Abkürzungen **PGM** (Programm) und **PVW** (Preview)

Der Windows-Build aus dem Original funktioniert weiterhin, zeigt jetzt aber ebenfalls die deutsche Oberfläche.

## Installation auf dem Mac

Kurzfassung (Details, Optionen und Fehlersuche in [docs/MACOS.md](docs/MACOS.md)):

1. Einmalig: Xcode Command Line Tools (`xcode-select --install`) und CMake 3.28 oder neuer installieren.
2. Projekt herunterladen, OBS beenden.
3. Im Terminal im Projektordner:

   ```bash
   bash scripts/build-macos.sh --install
   ```

4. OBS starten, Menü **Werkzeuge → Sunjoo OBS Link Multiview**.

Das Skript erkennt Version und Architektur der installierten OBS-App, lädt die passenden OBS-Header und
Qt-Dateien (mit Prüfsumme), baut das Plugin und installiert es nach
`~/Library/Application Support/obs-studio/plugins/obs-multiview-plus.plugin`.
Jeder Mac baut sich sein Plugin selbst; nach einem OBS-Update mit neuer Qt-Version einfach neu bauen.

Entfernen: den Ordner `obs-multiview-plus.plugin` aus `~/Library/Application Support/obs-studio/plugins` löschen.

## Bedienung

- **Preset ① – Szenen/Quellen 4er-Raster:** vier Kacheln im 2×2-Raster, ohne PGM/PVW.
- **Preset ② – ATEM-Stil (Standard):** oben links PVW, oben rechts PGM, darunter 8 Kacheln in 4 Spalten × 2 Zeilen.
- In den **Layout-Einstellungen** gibt es zusätzlich Querformat 16:9 / Hochformat 9:16 und gleichmäßige
  4er-, 9er- und 16er-Raster.
- Jede Kachel kann PGM, PVW, Szene, Quelle, OBS-Statistik, Leer, Uhr oder CPU / GPU (OBS) sein.
- Insgesamt sind höchstens 16 Kacheln möglich, PGM und PVW mitgezählt.
- In der Layout-Vorschau links im Einstellungsdialog eine Kachel auf eine andere ziehen, um die Positionen
  zu tauschen. In der Tabelle rechts werden Typ, Szene/Quelle, Anzeigename und Tally-Bezugsquelle festgelegt.
- **Wird beim ATEM-Stil PGM oder PVW auf eine kleine Kachel gezogen, wandern die zwei großen Bilder nach
  unten und die acht kleinen nach oben.** In umgekehrter Richtung zurückziehen stellt die ursprüngliche
  Anordnung wieder her.
- Gleich große Kacheln tauschen einzeln ihre Position. Name, gewählte Szene/Quelle und Tally-Bezug bleiben
  erhalten. **Speichern** übernimmt die Änderung, **Abbrechen** behält die bisherige Anordnung.
- Mit **Preset exportieren / Preset importieren** lassen sich eigene Layouts als JSON-Datei sichern und
  übertragen, auch zwischen Windows und Mac.
- Monitor auswählen und **Vollbild** drücken. **Esc** oder das Rechtsklick-Menü (**Fenstermodus**) kehrt
  zum Fenster zurück.
- **Szene per Klick wählen** ist standardmäßig aus. Eingeschaltet wählt ein Klick im Studio-Modus die PVW-Szene,
  im normalen Modus wechselt er die aktuelle Szene.
- **Doppelklick schaltet Sendung um** ist eine eigene Option. Im Studio-Modus führt ein Doppelklick den
  aktuellen OBS-Übergang aus, im normalen Modus wechselt er die aktuelle Szene.
- Ein Klick auf eine Quellen-Kachel ändert nie die Sendeszene.

Die Namensschrift ist immer weiß. Der Hintergrund des Namensfelds ist schwarz, bei PGM rot und bei PVW grün.
Ist eine Kachel in beiden enthalten, hat PGM Vorrang: Hintergrund und alle vier Rahmenseiten werden rot.

## Zusammenspiel mit Sunjoo OBS Link Controller

Kameras hinter dem Source Switcher von ME1 und den privaten Fade-Ausgängen von ME2 bis ME8 werden über den
tatsächlichen Ausgabepfad verfolgt. Öffentliche Szenennamen oder die ME-Auswahl werden nicht als Tally-Bezug
erraten.

- Verfolgt werden die in PGM/PVW tatsächlich sichtbaren Szenen, Gruppen und aktiven Quellen. Die bloße
  Auswahl in einer ME, die nicht auf Sendung ist, schaltet kein Rot.
- Durchsucht werden private Views und Gruppen je Kamera bis zur Originalkamera; versteckte Ablage-Szenen
  für Eingänge werden ignoriert.
- Während eines Fade-MIX werden die tatsächlich beteiligte alte und neue Kamera gemeinsam angezeigt; nach
  Abschluss des Videoübergangs wird die alte Kamera freigegeben, auch wenn ein Audio-Nachlauf noch auf sie
  verweist.
- ME1-Kameraszenen und ME2-Originalkameras werden über die bestehende **Tally-Bezugsquelle** verknüpft.
  Für Szenen mit nur einer Videoquelle bleibt die automatische Zuordnung erhalten.
- Ist eine Kamera auf irgendeinem der Ausgabepfade tatsächlich sichtbar, gilt sie als enthalten. PGM-Vorrang,
  Presets und Klick-Einstellungen bleiben unverändert.
- Es werden keine Schein-Szenen oder Tally-Signalquellen angelegt und keine Controller-Auswahl verändert.

Weitere Details zur Prüfung (auf Koreanisch): [Controller-Prüfbericht](docs/04-report/controller-integration.report.md).

## Tally und Statistik

Rot bedeutet: in PGM enthalten, grün: in PVW enthalten. Ist beides der Fall, hat PGM Vorrang und der ganze
Rahmen wird rot. Maßgeblich ist nicht die Markierung in der Szenenliste, sondern **der Sichtbarkeitsstatus
(Augensymbol) und der Ausgabepfad von Sendung bzw. Preview**. Verschachtelte Szenen und Gruppen werden
entlang der sichtbaren Pfade durchsucht (höchstens 8192 Knoten); Quellen unter einem ausgeblendeten Eltern-
element schalten kein Tally.

Beispiel mit der Struktur `+cam2 #02 → cam02` – alle folgenden Fälle werden erkannt:

1. Die Szene `+cam2 #02` liegt auf PGM.
2. Nur der Pfad `PGM → Gruppe CAM-PGM → +cam2 #02 → cam02` ist sichtbar.
3. Die Kamera wird direkt gezeigt, z. B. `Unten-PGM → Gruppe CAM-PGM-B → cam02`.
4. Eine andere Szene verschachtelt `Unten-PGM` und zeigt darin `cam02` über eine Gruppe.

Auch in Fall 3 und 4 leuchtet das Tally der Kachel `+cam2 #02`. Die automatische Zuordnung greift, wenn in
der Kameraszene **genau eine sichtbare Videoquelle** am Ende des Pfads liegt. Bei zusammengesetzten Szenen
mit Kamera, Logo usw. in den Einstellungen als **Tally-Bezugsquelle** `cam02` angeben. So leuchten nicht
alle Kamera-Tallys, nur weil mehrere Szenen dasselbe Logo verwenden. Mit festgelegter Bezugsquelle zählt nur
deren Sichtbarkeit; wird sie gelöscht, bleibt das Tally aus.

Während eines Fade-Übergangs gelten die tatsächlich beteiligte vorherige und nächste Szene als PGM. Bei
anderen Übergängen zählt der aktive Pfad. Ob Pixel von anderen Quellen verdeckt oder durch Filter
transparent sind, wird nicht berechnet. Ist der Studio-Modus aus, zeigt die PVW-Kachel wie das eingebaute
OBS-Multiview das Programmbild.

Die Statistik-Kachel zeigt FPS, verzögerte Frames, durchschnittliche Renderzeit, Stream-Status und
Netzwerk-Drops. Eine Kachel vom Typ **Uhr** zeigt Datum und Uhrzeit des Rechners. **CPU / GPU (OBS)** zeigt
im Sekundentakt die CPU-Auslastung des OBS-Prozesses (bezogen auf alle logischen Kerne), nicht die des
ganzen Rechners. Die GPU-Auslastung gibt es nur unter Windows (meistbelastete GPU-Engine); auf dem Mac steht
dort „nicht messbar“. Encoder-Verzögerung, Bitrate und Audiopegel sind nicht enthalten.

Die Einstellungen liegen im Plugin-Einstellungsordner von OBS unter `obs-multiview-plus/layout.json`
(Mac: `~/Library/Application Support/obs-studio/plugin_config/obs-multiview-plus/layout.json`).
Szenen und Quellen werden über ihre UUID erkannt. Gelöschte Ziele oder Ziele aus einer anderen
Szenensammlung müssen in den Einstellungen neu zugewiesen werden.

## Windows-Build (aus dem Original)

Voraussetzungen: Windows x64, Visual Studio 2022 C++ Build Tools und Windows SDK, CMake 3.28 oder neuer,
Node.js 20 oder neuer, OBS 32.2.2.

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
```

Das Skript lädt die OBS-32.2.2-Header und Qt 6.11.1 nach `.deps` und prüft die SHA256-Summen. Installiert
wird, indem die Ordner `obs-plugins` und `data` aus der erzeugten ZIP-Datei in den OBS-Ordner
(standardmäßig `C:\Program Files\obs-studio`) kopiert werden. Der isolierte Laufzeittest
(`scripts/test-runtime.ps1`) ist nur unter Windows verfügbar.

## Dokumentation, Prüfung und Lizenz

- [docs/MACOS.md](docs/MACOS.md) – Bauen, Installieren und Fehlersuche auf dem Mac (Deutsch)
- [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md), [docs/OBS_LINK_SUITE.md](docs/OBS_LINK_SUITE.md) und die
  Berichte unter `docs/` stammen aus dem Original und sind auf Koreanisch.

Ein erfolgreicher Build bedeutet nicht, dass Leistung und Tally an der echten Produktionsanlage geprüft sind.
Bitte vor dem Einsatz in einer Sendung mit dem eigenen Setup testen.

Lizenz: GPL-2.0-or-later, siehe `LICENSE`. Original © SunjooAn.

Verwendete offizielle OBS-Vorlagen:
[OBS Multiview](https://github.com/obsproject/obs-studio/blob/32.2.2/frontend/components/Multiview.cpp),
[OBS Projector](https://github.com/obsproject/obs-studio/blob/32.2.2/frontend/widgets/OBSProjector.cpp),
[Frontend API](https://github.com/obsproject/obs-studio/blob/32.2.2/frontend/api/obs-frontend-api.h).
