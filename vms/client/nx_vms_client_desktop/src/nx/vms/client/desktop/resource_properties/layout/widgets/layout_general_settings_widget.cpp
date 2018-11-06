#include "layout_general_settings_widget.h"
#include "ui_layout_general_settings_widget.h"

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/helper.h>

#include <ui/workaround/widgets_signals_workaround.h>

#include "../redux/layout_settings_dialog_store.h"
#include "../redux/layout_settings_dialog_state.h"
#include <nx/vms/client/desktop/common/widgets/hint_button.h>

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
    setHelpTopic(ui->lockedCheckBox, Qn::LayoutSettings_Locking_Help);

    const QString fixedSizeSuffix = " " + tr("cells");
    ui->fixedWidthSpinBox->setSuffix(fixedSizeSuffix);
    ui->fixedHeightSpinBox->setSuffix(fixedSizeSuffix);

    auto logicalIdHint = nx::vms::client::desktop::HintButton::hintThat(ui->logicalIdGroupBox);
    logicalIdHint->addHintLine(
        tr("Custom number that can be assigned to a layout for quick identification and access"));
}

void LayoutGeneralSettingsWidget::loadState(const LayoutSettingsDialogState& state)
{
    ui->lockedCheckBox->setChecked(state.locked);
    ui->logicalIdSpinBox->setValue(state.logicalId);
    ui->fixedSizeGroupBox->setChecked(state.fixedSizeEnabled);
    ui->fixedWidthSpinBox->setValue(state.fixedSize.width());
    ui->fixedHeightSpinBox->setValue(state.fixedSize.height());
}

} // namespace nx::vms::client::desktop
