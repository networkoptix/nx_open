#include "attach_to_videowall_dialog.h"
#include "ui_attach_to_videowall_dialog.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QRadioButton>

#include <ui/workaround/qt5_combobox_workaround.h>

QnAttachToVideowallDialog::QnAttachToVideowallDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnAttachToVideowallDialog)
{
    ui->setupUi(this);
}

QnAttachToVideowallDialog::~QnAttachToVideowallDialog(){}

void QnAttachToVideowallDialog::loadFromResource(const QnVideoWallResourcePtr &videowall) {
    ui->manageWidget->loadFromResource(videowall);
}

void QnAttachToVideowallDialog::submitToResource(const QnVideoWallResourcePtr &videowall) {
    ui->manageWidget->submitToResource(videowall);
}

