#include "attach_to_videowall_dialog.h"
#include "ui_attach_to_videowall_dialog.h"

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

QnVideowallAttachSettings QnAttachToVideowallDialog::settings() const {
    QnVideowallAttachSettings result;
    if (ui->layoutCustom->isChecked())
        result.layoutMode = QnVideowallAttachSettings::LayoutCustom;
    else if (ui->layoutClone->isChecked())
        result.layoutMode = QnVideowallAttachSettings::LayoutClone;
    else
        result.layoutMode = QnVideowallAttachSettings::LayoutNone;
    result.layoutId = ui->layoutsComboBox->currentData().toInt();

    return result;
}

void QnAttachToVideowallDialog::loadSettings(const QnVideowallAttachSettings &settings) {

}

void QnAttachToVideowallDialog::loadLayoutsList(const QnLayoutResourceList &layouts) {
    foreach (const QnLayoutResourcePtr &layout, layouts) {
        ui->layoutsComboBox->addItem(layout->getName(), layout->getId().toInt());
    }
    ui->layoutCustom->setEnabled(!layouts.isEmpty());
}
