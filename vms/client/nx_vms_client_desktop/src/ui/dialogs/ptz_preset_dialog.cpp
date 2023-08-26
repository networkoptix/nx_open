// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ptz_preset_dialog.h"
#include "ui_ptz_preset_dialog.h"

#include <QtWidgets/QPushButton>

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_data.h>
#include <core/ptz/ptz_preset.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <utils/common/variant.h>

using namespace nx::vms::client::core::ptz;
using namespace nx::vms::client::desktop;
using namespace nx::vms::common::ptz;

QnPtzPresetDialog::QnPtzPresetDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::PtzPresetDialog())
{
    ui->setupUi(this);

    setHelpTopic(this, HelpTopic::Id::PtzPresets);

    connect(ui->nameEdit, SIGNAL(textChanged(const QString &)), this, SLOT(updateOkButtonEnabled()));

    updateOkButtonEnabled();
}

QnPtzPresetDialog::~QnPtzPresetDialog() {
}

void QnPtzPresetDialog::loadData(const QnPtzData &data)
{
    ui->nameEdit->setText(tr("Saved Position %1").arg(data.presets.size() + 1));

    if (!m_hotkeysDelegate)
        return;

    QList<int> hotkeys;
    for(int i = 1; i < 10; i++)
        hotkeys.push_back(i);
    hotkeys.push_back(0);

    PresetIdByHotkey usedHotkeys = m_hotkeysDelegate->hotkeys();
    foreach (const QnPtzPreset &preset, data.presets) {
        int key = usedHotkeys.key(preset.id, kNoHotkey);
        if (key == kNoHotkey)
            continue;
        hotkeys.removeOne(key);
    }
    foreach (const QnPtzTour &tour, data.tours) {
        int key = usedHotkeys.key(tour.id, kNoHotkey);
        if (key == kNoHotkey)
            continue;
        hotkeys.removeOne(key);
    }

    ui->hotkeyComboBox->clear();
    ui->hotkeyComboBox->addItem(tr("None"), kNoHotkey);
    foreach(int hotkey, hotkeys)
        ui->hotkeyComboBox->addItem(QString::number(hotkey), hotkey);
    setHotkey(hotkeys.isEmpty() ? -1 : hotkeys.first());
}

void QnPtzPresetDialog::saveData() {
    // TODO: #sivanov Ask to replace if there is a preset with the same name?
    QString presetId = QnUuid::createUuid().toString();
    createPreset(QnPtzPreset(presetId, ui->nameEdit->text()));

    if (!m_hotkeysDelegate || hotkey() < 0)
        return;

    PresetIdByHotkey hotkeys = m_hotkeysDelegate->hotkeys();
    hotkeys[hotkey()] = presetId;
    m_hotkeysDelegate->updateHotkeys(hotkeys);
    return;
}

DataFields QnPtzPresetDialog::requiredFields() const
{
    return DataField::presets | DataField::tours;
}

void QnPtzPresetDialog::updateFields(DataFields fields) {
    Q_UNUSED(fields)
}

QnAbstractPtzHotkeyDelegate* QnPtzPresetDialog::hotkeysDelegate() const {
    return m_hotkeysDelegate;
}

void QnPtzPresetDialog::setHotkeysDelegate(QnAbstractPtzHotkeyDelegate *delegate) {
    m_hotkeysDelegate = delegate;
}

int QnPtzPresetDialog::hotkey() const {
    return ui->hotkeyComboBox->itemData(ui->hotkeyComboBox->currentIndex()).toInt();
}

void QnPtzPresetDialog::setHotkey(int hotkey) {
    int index = ui->hotkeyComboBox->findData(hotkey);
    if(index < 0)
        index = ui->hotkeyComboBox->findData(kNoHotkey);
    ui->hotkeyComboBox->setCurrentIndex(index);
}

void QnPtzPresetDialog::updateOkButtonEnabled() {
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!ui->nameEdit->text().isEmpty());
}
