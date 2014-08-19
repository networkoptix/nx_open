#include "videowall_settings_dialog.h"
#include "ui_videowall_settings_dialog.h"

#include <core/resource/videowall_resource.h>

QnVideowallSettingsDialog::QnVideowallSettingsDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnVideowallSettingsDialog)
{
    ui->setupUi(this);
}

QnVideowallSettingsDialog::~QnVideowallSettingsDialog() {}

void QnVideowallSettingsDialog::loadFromResource(const QnVideoWallResourcePtr &videowall) {
    ui->autoRunCheckBox->setChecked(videowall->isAutorun());
}

void QnVideowallSettingsDialog::submitToResource(const QnVideoWallResourcePtr &videowall) const {
    videowall->setAutorun(ui->autoRunCheckBox->isChecked());
}

bool QnVideowallSettingsDialog::isCreateShortcut() const {
    return ui->shortcutCheckbox->isChecked();
}

void QnVideowallSettingsDialog::setCreateShortcut(bool value) {
    ui->shortcutCheckbox->setChecked(value);
}

bool QnVideowallSettingsDialog::isShortcutsSupported() const {
    return ui->shortcutCheckbox->isVisible();
}

void QnVideowallSettingsDialog::setShortcutsSupported(bool value) {
    ui->shortcutCheckbox->setVisible(value);
}
