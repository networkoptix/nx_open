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

QnVideowallAttachSettings QnAttachToVideowallDialog::settings() const {
    QnVideowallAttachSettings result;
    return result;
}

void QnAttachToVideowallDialog::loadSettings(const QnVideowallAttachSettings &settings) {

}

void QnAttachToVideowallDialog::loadLayoutsList(const QnLayoutResourceList &layouts) {

}
