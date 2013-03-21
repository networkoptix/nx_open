#include "layout_settings_dialog.h"
#include "ui_layout_settings_dialog.h"

#include <core/resource/layout_resource.h>

QnLayoutSettingsDialog::QnLayoutSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnLayoutSettingsDialog)
{
    ui->setupUi(this);
}

QnLayoutSettingsDialog::~QnLayoutSettingsDialog()
{
}

void QnLayoutSettingsDialog::readFromResource(const QnLayoutResourcePtr &layout) {

}

void QnLayoutSettingsDialog::submitToResource(const QnLayoutResourcePtr &layout) {

}
