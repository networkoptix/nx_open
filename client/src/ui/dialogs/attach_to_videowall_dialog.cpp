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

    connect(ui->layoutsComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [&](){ui->layoutCustom->setChecked(true);});
    connect(ui->amAllRadioButton, &QRadioButton::toggled, ui->autoFillCheckBox, &QCheckBox::setDisabled);

#ifndef Q_OS_WIN
    ui->autoRunCheckBox->setVisible(false);
#endif // !Q_OS_WIN

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
    result.layoutId = ui->layoutsComboBox->currentData().value<QUuid>();

    if (ui->amAllRadioButton->isChecked())
        result.attachMode = QnVideowallAttachSettings::AttachAll;
    else if (ui->amScreenRadioButton->isChecked())
        result.attachMode = QnVideowallAttachSettings::AttachScreen;
    else
        result.attachMode = QnVideowallAttachSettings::AttachWindow;

    result.autoFill = ui->autoFillCheckBox->isChecked();
    result.closeClient = ui->closeClientCheckBox->isChecked();

    return result;
}

void QnAttachToVideowallDialog::loadSettings(const QnVideowallAttachSettings &settings) {
    if (ui->layoutsComboBox->count() > 0)
        ui->layoutsComboBox->setCurrentIndex(qMax(ui->layoutsComboBox->findData(settings.layoutId), 0));

    switch (settings.layoutMode) {
    case QnVideowallAttachSettings::LayoutNone:
        ui->layoutNone->setChecked(true);
        break;
    case QnVideowallAttachSettings::LayoutClone:
        ui->layoutClone->setChecked(ui->layoutClone->isEnabled());
        break;
    case QnVideowallAttachSettings::LayoutCustom:
        ui->layoutCustom->setChecked(ui->layoutCustom->isEnabled());
        break;
    default:
        break;
    }

    switch (settings.attachMode) {
    case QnVideowallAttachSettings::AttachWindow:
        ui->amWindowRadioButton->setChecked(true);
        break;
    case QnVideowallAttachSettings::AttachScreen:
        ui->amScreenRadioButton->setChecked(true);
        break;
    case QnVideowallAttachSettings::AttachAll:
        ui->amAllRadioButton->setChecked(true);
        break;
    default:
        break;
    }

    ui->autoFillCheckBox->setChecked(settings.autoFill);
    ui->closeClientCheckBox->setChecked(settings.closeClient);
}

void QnAttachToVideowallDialog::loadLayoutsList(const QnLayoutResourceList &layouts) {
    foreach (const QnLayoutResourcePtr &layout, layouts) {
        ui->layoutsComboBox->addItem(layout->getName(), layout->getId());
    }
    ui->layoutCustom->setEnabled(!layouts.isEmpty());
}

void QnAttachToVideowallDialog::setCanClone(bool canClone) {
    ui->layoutClone->setEnabled(canClone);
}