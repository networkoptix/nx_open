#include "ptz_preset_dialog.h"
#include "ui_ptz_preset_dialog.h"

#include <utils/common/variant.h>

QnPtzPresetDialog::QnPtzPresetDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::PtzPresetDialog())
{
    ui->setupUi(this);

    QList<int> hotkeys;
    for(int i = 0; i < 10; i++)
        hotkeys.push_back(i);
    setHotkeys(hotkeys);
}

QnPtzPresetDialog::~QnPtzPresetDialog() {
    return;
}

QnPtzPreset QnPtzPresetDialog::preset() const {
    QnPtzPreset result = m_preset;
    result.name = ui->nameEdit->text();
    result.hotkey = currentHotkey();
    return result;
}

void QnPtzPresetDialog::setPreset(const QnPtzPreset &preset) {
    m_preset = preset;
    ui->nameEdit->setText(preset.name);
    setCurrentHotkey(preset.hotkey);
}

const QList<int> &QnPtzPresetDialog::hotkeys() const {
    return m_hotkeys;
}

void QnPtzPresetDialog::setHotkeys(const QList<int> &hotkeys) {
    if(m_hotkeys == hotkeys)
        return;

    m_hotkeys = hotkeys;

    bool haveHotkeys = !m_hotkeys.isEmpty();
    ui->hotkeyComboBox->setVisible(haveHotkeys);
    ui->hotkeyLabel->setVisible(haveHotkeys);
    if(!haveHotkeys)
        return;

    int currentHotkey = this->currentHotkey();

    ui->hotkeyComboBox->clear();
    ui->hotkeyComboBox->addItem(tr("None"), -1);
    foreach(int hotkey, m_hotkeys)
        ui->hotkeyComboBox->addItem(QString::number(hotkey), hotkey);

    setCurrentHotkey(currentHotkey);
}

int QnPtzPresetDialog::currentHotkey() const {
    return qvariant_cast<int>(ui->hotkeyComboBox->itemData(ui->hotkeyComboBox->currentIndex()), -1);
}

void QnPtzPresetDialog::setCurrentHotkey(int hotkey) {
    int index = ui->hotkeyComboBox->findData(hotkey);
    if(index < 0)
        index = ui->hotkeyComboBox->findData(-1);
    ui->hotkeyComboBox->setCurrentIndex(index);
}
