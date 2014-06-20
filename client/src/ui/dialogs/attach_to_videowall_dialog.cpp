#include "attach_to_videowall_dialog.h"
#include "ui_attach_to_videowall_dialog.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QRadioButton>

#include <core/resource/layout_resource.h>

QnAttachToVideowallDialog::QnAttachToVideowallDialog(QWidget *parent) :
    QnButtonBoxDialog(parent),
    ui(new Ui::QnAttachToVideowallDialog)
{
    ui->setupUi(this);
}

QnAttachToVideowallDialog::~QnAttachToVideowallDialog(){}

void QnAttachToVideowallDialog::loadFromResource(const QnVideoWallResourcePtr &videowall) {
    ui->previewWidget->loadFromResource(videowall);
}

void QnAttachToVideowallDialog::submitToResource(const QnVideoWallResourcePtr &videowall) {
    ui->previewWidget->submitToResource(videowall);
}

