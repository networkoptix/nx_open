#include "attach_to_videowall_dialog.h"
#include "ui_attach_to_videowall_dialog.h"

#include <core/resource/layout_resource.h>

QnAttachToVideowallDialog::QnAttachToVideowallDialog(QWidget *parent) :
    QnButtonBoxDialog(parent),
    ui(new Ui::QnAttachToVideowallDialog)
{
    ui->setupUi(this);


}

QnAttachToVideowallDialog::~QnAttachToVideowallDialog(){}

QnAttachToVideowallDialog::AttachSettings QnAttachToVideowallDialog::settings() const {

}

void QnAttachToVideowallDialog::loadSettings(const QnAttachToVideowallDialog::AttachSettings &settings) {

}

void QnAttachToVideowallDialog::loadLayoutsList(const QnLayoutResourceList &layouts) {

}
