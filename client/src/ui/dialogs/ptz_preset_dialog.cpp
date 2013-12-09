#include "ptz_preset_dialog.h"
#include "ui_ptz_preset_dialog.h"

#include <QtWidgets/QPushButton>

#include <utils/common/variant.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

QnPtzPresetDialog::QnPtzPresetDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::PtzPresetDialog())
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::PtzPresets_Help);

    connect(ui->nameEdit, SIGNAL(textChanged(const QString &)), this, SLOT(updateOkButtonEnabled()));

    setForbiddenHotkeys(QList<int>(), true);
    updateOkButtonEnabled();
}

QnPtzPresetDialog::~QnPtzPresetDialog() {
    return;
}

QString QnPtzPresetDialog::name() const {
    return ui->nameEdit->text();
}

void QnPtzPresetDialog::setName(const QString &name) {
    ui->nameEdit->setText(name);
}

int QnPtzPresetDialog::hotkey() const {
    return qvariant_cast<int>(ui->hotkeyComboBox->itemData(ui->hotkeyComboBox->currentIndex()), -1);
}

void QnPtzPresetDialog::setHotkey(int hotkey) {
    int index = ui->hotkeyComboBox->findData(hotkey);
    if(index < 0)
        index = ui->hotkeyComboBox->findData(-1);
    ui->hotkeyComboBox->setCurrentIndex(index);
}

const QList<int> &QnPtzPresetDialog::forbiddenHotkeys() const {
    return m_forbiddenHotkeys;
}

void QnPtzPresetDialog::setForbiddenHotkeys(const QList<int> &forbiddenHotkeys) {
    setForbiddenHotkeys(forbiddenHotkeys, false);
}

void QnPtzPresetDialog::setForbiddenHotkeys(const QList<int> &forbiddenHotkeys, bool force) {
    if(!force && m_forbiddenHotkeys == forbiddenHotkeys)
        return;

    m_forbiddenHotkeys = forbiddenHotkeys;

    QList<int> hotkeys;
    for(int i = 0; i < 10; i++)
        hotkeys.push_back(i);
    foreach(int forbiddenHotkey, m_forbiddenHotkeys)
        hotkeys.removeOne(forbiddenHotkey);

    bool haveHotkeys = !hotkeys.isEmpty();
    ui->hotkeyComboBox->setVisible(haveHotkeys);
    ui->hotkeyLabel->setVisible(haveHotkeys);
    if(!haveHotkeys)
        return;

    int currentHotkey = this->hotkey();

    ui->hotkeyComboBox->clear();
    ui->hotkeyComboBox->addItem(tr("None"), -1);
    foreach(int hotkey, hotkeys)
        ui->hotkeyComboBox->addItem(QString::number(hotkey), hotkey);

    setHotkey(currentHotkey);
}

void QnPtzPresetDialog::updateOkButtonEnabled() {
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!ui->nameEdit->text().isEmpty());
}
