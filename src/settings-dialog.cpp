#include "settings-dialog.hpp"
#include "layout-preview.hpp"
#include "version.hpp"
#include <obs.h>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <algorithm>

namespace mv {
namespace {
struct SourceChoice { QString name; QString uuid; bool scene; };
bool collectSource(void *data, obs_source_t *source)
{
    const char *uuid = obs_source_get_uuid(source);
    const char *name = obs_source_get_name(source);
    if (uuid && name && (obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO))
        static_cast<QVector<SourceChoice> *>(data)->push_back({QString::fromUtf8(name), QString::fromUtf8(uuid), obs_source_is_scene(source)});
    return true;
}
}
SettingsDialog::SettingsDialog(QWidget *parent, const LayoutConfig &config) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Multiview-Einstellungen")); resize(1200, 740);
    auto *root = new QVBoxLayout(this);
    auto *top = new QHBoxLayout;
    ratio_ = new QComboBox(this); ratio_->addItems({QStringLiteral("Querformat 16:9"), QStringLiteral("Hochformat 9:16")});
    preset_ = new QComboBox(this);
    preset_->addItem(QStringLiteral("① Szenen/Quellen 4er-Raster (ohne PGM/PVW)"), "sources4");
    preset_->addItem(QStringLiteral("② ATEM-Stil: PVW/PGM + 8 Kacheln"), "atem8");
    preset_->addItem(QStringLiteral("Gleichmäßig 4er-Raster"), "grid4");
    preset_->addItem(QStringLiteral("Gleichmäßig 9er-Raster"), "grid9");
    preset_->addItem(QStringLiteral("Gleichmäßig 16er-Raster"), "grid16");


    preset_->setCurrentIndex(1);
    auto *applyPreset = new QPushButton(QStringLiteral("Preset anwenden"), this);


    top->addWidget(ratio_); top->addWidget(preset_); top->addWidget(applyPreset);

    top->addStretch(); root->addLayout(top);
    auto *options = new QHBoxLayout;
    names_ = new QCheckBox(QStringLiteral("Namen anzeigen"), this);
    click_ = new QCheckBox(QStringLiteral("Szene per Klick wählen"), this);
    doubleClick_ = new QCheckBox(QStringLiteral("Doppelklick schaltet Sendung um"), this);
    options->addWidget(names_); options->addWidget(click_); options->addWidget(doubleClick_); options->addStretch();
    root->addLayout(options);
    auto *help = new QLabel(QStringLiteral("Ziehe links im Layout eine Kachel an eine andere Stelle, um die Position zu ändern. Wird eine große Anzeige in den Bereich der kleinen gezogen, tauschen beide Bereiche komplett.\nKlick: Im Studio-Modus wird PVW gewählt, im normalen Modus die aktuelle Szene gewechselt. Der Doppelklick-Wechsel ist eine eigene Option."), this);
    help->setWordWrap(true); root->addWidget(help);
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    preview_ = new LayoutPreview(splitter);
    table_ = new QTableWidget(0, 4, splitter);
    table_->setHorizontalHeaderLabels({QStringLiteral("Typ"), QStringLiteral("Szene / Quelle"), QStringLiteral("Anzeigename (optional)"), QStringLiteral("Tally-Bezugsquelle")});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    splitter->addWidget(preview_); splitter->addWidget(table_); splitter->setSizes({400, 760});
    root->addWidget(splitter, 1);
    connect(preview_, &LayoutPreview::tileSelected, this, [this](int row) { table_->selectRow(row); });
    connect(table_, &QTableWidget::itemSelectionChanged, this, [this] { preview_->setSelectedTile(table_->currentRow()); });
    connect(preview_, &LayoutPreview::tilesDropped, this, [this](int from, int to) {
        auto c = resultConfig();
        if (!swapLayoutTiles(c, from, to)) {
            QMessageBox::information(this, QStringLiteral("Positionen tauschen"), QStringLiteral("Diese beiden Kacheln lassen sich nicht tauschen. Bitte Kacheln gleicher Größe oder den oberen bzw. unteren Bereich verwenden.")); return;
        }
        geometry_ = c; updatePreview();
    });
    connect(ratio_, &QComboBox::currentIndexChanged, this, [this] { updatePreview(); });
    auto *actions = new QHBoxLayout;
    auto *add = new QPushButton(QStringLiteral("Leere Kachel hinzufügen"), this);
    auto *remove = new QPushButton(QStringLiteral("Ausgewählte Kacheln löschen"), this);
    auto *refresh = new QPushButton(QStringLiteral("Szenen-/Quellenliste aktualisieren"), this);
    actions->addWidget(add); actions->addWidget(remove); actions->addWidget(refresh); actions->addStretch(); root->addLayout(actions);
    auto *identity = new QLabel(QString::fromUtf8(Identity), this);
    identity->setObjectName("creatorVersionLabel");
    identity->setTextFormat(Qt::PlainText);
    actions->addWidget(identity);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("Speichern"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Abbrechen")); root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        const auto issue = validateLayout(resultConfig());
        if (!issue.isEmpty()) { QMessageBox::warning(this, QStringLiteral("Layout prüfen"), issue); return; }
        accept();
    });
    connect(applyPreset, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, QStringLiteral("Preset anwenden"), QStringLiteral("Aktuelle Kachelanordnung und Quellenauswahl durch das Preset ersetzen?")) != QMessageBox::Yes) return;
        auto c = presetLayout(preset_->currentData().toString(), ratio_->currentIndex() == 1);
        c.showNames = names_->isChecked(); c.clickSwitch = click_->isChecked(); c.doubleClickTransition = doubleClick_->isChecked(); load(c);
    });
    connect(add, &QPushButton::clicked, this, [this] {
        if (table_->rowCount() >= 16) { QMessageBox::information(this, QStringLiteral("Kachellimit"), QStringLiteral("Es sind höchstens 16 Kacheln möglich.")); return; }
        const auto c = resultConfig();
        for (int y = 0; y < c.rows; ++y) for (int x = 0; x < c.columns; ++x) {
            bool occupied = false;
            for (const auto &t : c.tiles) if (x >= t.x && x < t.x + t.w && y >= t.y && y < t.y + t.h) occupied = true;
            if (!occupied) { TileConfig t; t.x = x; t.y = y; geometry_.tiles.push_back(t); appendTile(t); updatePreview(); return; }
        }
        QMessageBox::information(this, QStringLiteral("Kein Platz frei"), QStringLiteral("Es ist kein Platz mehr frei. Lösche eine bestehende Kachel oder wähle ein anderes Preset."));
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        // Rebuild after removal so per-row signal captures always match their row.
        auto c = resultConfig(); auto selected = table_->selectionModel()->selectedRows();
        std::sort(selected.begin(), selected.end(), [](const QModelIndex &a, const QModelIndex &b) { return a.row() > b.row(); });
        for (const auto &index : selected) c.tiles.removeAt(index.row()); load(c);
    });
    connect(refresh, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < table_->rowCount(); ++i) {
            auto *box = static_cast<QComboBox *>(table_->cellWidget(i, 1)); populateSources(i, box->currentData().toString());
        }
        updatePreview();
    });
    load(config);
}
void SettingsDialog::load(const LayoutConfig &c)
{
    loading_ = true; geometry_ = c;
    ratio_->setCurrentIndex(c.portrait ? 1 : 0); names_->setChecked(c.showNames);
    click_->setChecked(c.clickSwitch); doubleClick_->setChecked(c.doubleClickTransition);
    table_->setRowCount(0);
    for (const auto &t : c.tiles) appendTile(t);
    loading_ = false; updatePreview();
}
void SettingsDialog::appendTile(const TileConfig &t)
{
    const int row = table_->rowCount(); table_->insertRow(row);
    auto *kind = new QComboBox(table_);
    kind->addItems({QStringLiteral("PGM"), QStringLiteral("PVW"), QStringLiteral("Szene"), QStringLiteral("Quelle"), QStringLiteral("Statistik"), QStringLiteral("Leer"), QStringLiteral("Uhr"), QStringLiteral("CPU / GPU (OBS)")});
    kind->setCurrentIndex(static_cast<int>(t.kind)); table_->setCellWidget(row, 0, kind);
    table_->setCellWidget(row, 1, new QComboBox(table_));
    auto *tally = new QComboBox(table_);
    tally->addItem(QStringLiteral("Automatisch (nach sichtbarem Pfad)"), QString());
    if (!t.tallyUuid.isEmpty()) tally->addItem(t.tallyUuid, t.tallyUuid);
    tally->setCurrentIndex(t.tallyUuid.isEmpty() ? 0 : 1);
    tally->setToolTip(QStringLiteral("Bei zusammengesetzten Szenen: Quelle angeben, um das Tally an der Sichtbarkeit einer bestimmten Kamera festzumachen."));
    table_->setCellWidget(row, 3, tally);
    auto *label = new QLineEdit(t.label, table_); label->setMaxLength(256); table_->setCellWidget(row, 2, label);
    populateSources(row, t.uuid);
    connect(kind, &QComboBox::currentIndexChanged, this, [this, row] { populateSources(row, {}); updatePreview(); });
    connect(label, &QLineEdit::textChanged, this, [this] { updatePreview(); });
    connect(static_cast<QComboBox *>(table_->cellWidget(row, 1)), &QComboBox::currentIndexChanged, this, [this] { updatePreview(); });
}
void SettingsDialog::populateSources(int row, const QString &uuid)
{
    auto *box = static_cast<QComboBox *>(table_->cellWidget(row, 1));
    const auto kind = static_cast<TileKind>(static_cast<QComboBox *>(table_->cellWidget(row, 0))->currentIndex());
    const QSignalBlocker blocker(box);
    box->clear(); box->addItem(QStringLiteral("Bitte wählen"), QString());
    const bool needsSource = kind == TileKind::Scene || kind == TileKind::Source; box->setEnabled(needsSource);
    QVector<SourceChoice> sources; obs_enum_sources(collectSource, &sources);
    // obs_enum_sources includes inputs; scenes are enumerated separately by libobs.
    obs_enum_scenes(collectSource, &sources);
    std::sort(sources.begin(), sources.end(), [](const SourceChoice &a, const SourceChoice &b) { return a.name.localeAwareCompare(b.name) < 0; });
    auto *tally = static_cast<QComboBox *>(table_->cellWidget(row, 3));
    const auto previousTally = tally->currentData().toString();
    tally->clear(); tally->addItem(QStringLiteral("Automatisch (nach sichtbarem Pfad)"), QString());
    for (const auto &s : sources) if (tally->findData(s.uuid) < 0) tally->addItem(s.name, s.uuid);
    int tallyIndex = tally->findData(previousTally);
    if (tallyIndex < 0 && !previousTally.isEmpty()) {
        tally->addItem(QStringLiteral("[fehlt] %1").arg(previousTally), previousTally); tallyIndex = tally->count() - 1;
    }
    tally->setCurrentIndex(tallyIndex < 0 ? 0 : tallyIndex);
    if (!needsSource) return;
    for (const auto &s : sources)
        if ((kind != TileKind::Scene || s.scene) && box->findData(s.uuid) < 0) box->addItem(s.name, s.uuid);
    int index = box->findData(uuid);
    if (index < 0 && !uuid.isEmpty()) { box->addItem(QStringLiteral("[fehlt] %1").arg(uuid), uuid); index = box->count() - 1; }
    box->setCurrentIndex(index < 0 ? 0 : index);
}
LayoutConfig SettingsDialog::resultConfig() const
{
    LayoutConfig c = geometry_; c.tiles.clear(); c.portrait = ratio_->currentIndex() == 1; c.showNames = names_->isChecked();
    c.clickSwitch = click_->isChecked(); c.doubleClickTransition = doubleClick_->isChecked();
    for (int row = 0; row < table_->rowCount(); ++row) {
        TileConfig t = geometry_.tiles.value(row); t.kind = static_cast<TileKind>(static_cast<QComboBox *>(table_->cellWidget(row, 0))->currentIndex());
        t.uuid = static_cast<QComboBox *>(table_->cellWidget(row, 1))->currentData().toString();
        t.label = static_cast<QLineEdit *>(table_->cellWidget(row, 2))->text();
        t.tallyUuid = static_cast<QComboBox *>(table_->cellWidget(row, 3))->currentData().toString();
        c.tiles.push_back(t);
    }
    return c;
}
void SettingsDialog::updatePreview()
{
    if (loading_) return;
    auto config = resultConfig();
    for (int row=0; row<config.tiles.size(); ++row) {
        auto &tile=config.tiles[row];
        if (tile.label.isEmpty() && (tile.kind==TileKind::Scene || tile.kind==TileKind::Source)) {
            const auto *box=static_cast<QComboBox *>(table_->cellWidget(row,1));
            if (!box->currentData().toString().isEmpty()) tile.label=box->currentText();
        }
    }
    preview_->setLayoutConfig(config);
}
}
