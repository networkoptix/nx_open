// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_general_settings_widget.h"
#include "ui_layout_general_settings_widget.h"

#include <nx/vms/client/desktop/common/widgets/hint_button.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include "../flux/layout_settings_dialog_state.h"
#include "../flux/layout_settings_dialog_store.h"

namespace nx::vms::client::desktop {

LayoutGeneralSettingsWidget::LayoutGeneralSettingsWidget(
    LayoutSettingsDialogStore* store,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::LayoutGeneralSettingsWidget)
{
    setupUi();
    connect(store, &LayoutSettingsDialogStore::stateChanged, this,
        &LayoutGeneralSettingsWidget::loadState);

    connect(ui->lockedCheckBox, &QCheckBox::clicked, store,
        &LayoutSettingsDialogStore::setLocked);
    connect(ui->logicalIdSpinBox, QnSpinboxIntValueChanged, store,
        &LayoutSettingsDialogStore::setLogicalId);
    connect(ui->resetLogicalIdButton, &QPushButton::clicked, store,
        &LayoutSettingsDialogStore::resetLogicalId);
    connect(ui->generateLogicalIdButton, &QPushButton::clicked, store,
        &LayoutSettingsDialogStore::generateLogicalId);
    connect(ui->fixedSizeGroupBox, &QGroupBox::clicked, store,
        &LayoutSettingsDialogStore::setFixedSizeEnabled);
    connect(ui->fixedWidthSpinBox, QnSpinboxIntValueChanged, store,
        &LayoutSettingsDialogStore::setFixedSizeWidth);
    connect(ui->fixedHeightSpinBox, QnSpinboxIntValueChanged, store,
        &LayoutSettingsDialogStore::setFixedSizeHeight);
}

LayoutGeneralSettingsWidget::~LayoutGeneralSettingsWidget()
{
}

void LayoutGeneralSettingsWidget::setupUi()
{
    ui->setupUi(this);
    ui->lockedCheckBox->setProperty(style::Properties::kCheckBoxAsButton, true);
    ui->lockedCheckBox->setForegroundRole(QPalette::ButtonText);
    setHelpTopic(ui->lockedCheckBox, HelpTopic::Id::LayoutSettings_Locking);

    const QString fixedSizeSuffix = " " + tr("cells");
    ui->fixedWidthSpinBox->setSuffix(fixedSizeSuffix);
    ui->fixedHeightSpinBox->setSuffix(fixedSizeSuffix);

    const auto logicalIdHint = HintButton::createGroupBoxHint(ui->logicalIdGroupBox);
    logicalIdHint->setHintText(
        tr("Custom number that can be assigned to a layout for quick identification and access"));

    setWarningStyleOn(ui->logicalIdWarningLabel);
    ui->logicalIdWarningLabel->setText(tr("This ID is already used in the System. "
        "Use Generate button to find a free ID."));
}

void LayoutGeneralSettingsWidget::loadState(const LayoutSettingsDialogState& state)
{
    ui->logicalIdGroupBox->setHidden(state.isCrossSystem);
    if (!state.isCrossSystem)
    {
        ui->logicalIdSpinBox->setValue(state.logicalId);
        const bool duplicateLogicalId = state.isDuplicateLogicalId();
        ui->logicalIdWarningLabel->setVisible(duplicateLogicalId);
        setWarningStyleOn(ui->logicalIdSpinBox, duplicateLogicalId);
    }

    ui->lockedCheckBox->setChecked(state.locked);
    ui->fixedSizeGroupBox->setChecked(state.fixedSizeEnabled);
    ui->fixedWidthSpinBox->setValue(state.fixedSize.width());
    ui->fixedHeightSpinBox->setValue(state.fixedSize.height());
}

} // namespace nx::vms::client::desktop
