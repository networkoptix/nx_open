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
    connect(ui->autoFillCheckBox, &QCheckBox::toggled,    this, &QnAttachToVideowallDialog::updatePreview);
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
    updatePreview();
}

void QnAttachToVideowallDialog::loadLayoutsList(const QnLayoutResourceList &layouts) {
    foreach (const QnLayoutResourcePtr &layout, layouts) {
        ui->layoutsComboBox->addItem(layout->getName(), layout->getId());
    }
    ui->layoutCustom->setEnabled(!layouts.isEmpty());
}

bool QnAttachToVideowallDialog::canClone() const {
    return ui->layoutClone->isEnabled();
}

void QnAttachToVideowallDialog::setCanClone(bool value) {
    ui->layoutClone->setEnabled(value);
}

bool QnAttachToVideowallDialog::isCreateShortcut() const {
    return ui->shortcutCheckbox->isChecked();
}

void QnAttachToVideowallDialog::setCreateShortcut(bool value) {
    ui->shortcutCheckbox->setChecked(value);
}

bool QnAttachToVideowallDialog::isShortcutsSupported() const {
    return ui->shortcutCheckbox->isVisible();
}

void QnAttachToVideowallDialog::setShortcutsSupported(bool value) {
    ui->shortcutCheckbox->setVisible(value);
}

void QnAttachToVideowallDialog::updatePreview() {
    ui->previewWidget->setAutoFill(ui->autoFillCheckBox->isChecked());
}

QnVideoWallResourcePtr QnAttachToVideowallDialog::videowall() const {
    return ui->previewWidget->videowall();
}

void QnAttachToVideowallDialog::setVideowall(const QnVideoWallResourcePtr &videowall) {
    ui->previewWidget->setVideowall(videowall);
}

