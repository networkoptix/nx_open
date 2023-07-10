// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_type_picker_widget.h"
#include "ui_action_type_picker_widget.h"

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/utils/common.h>
#include <nx/vms/rules/utils/string_helper.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop::rules {

ActionTypePickerWidget::ActionTypePickerWidget(SystemContext* context, QWidget* parent):
    QWidget(parent),
    SystemContextAware{context},
    ui(new Ui::ActionTypePickerWidget())
{
    ui->setupUi(this);
    setPaletteColor(ui->doLabel, QPalette::WindowText, core::colorTheme()->color("light1"));

    const auto servers = systemContext()->resourcePool()->servers();
    for (const auto& actionDescriptor: systemContext()->vmsRulesEngine()->actions())
    {
        if (!vms::rules::utils::hasItemSupportedServer(servers, actionDescriptor))
            continue;

        ui->actionTypeComboBox->addItem(actionDescriptor.displayName, actionDescriptor.id);
    }

    connect(ui->actionTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this]()
        {
            emit actionTypePicked(actionType());
        });
}

// Non-inline destructor is required for member scoped pointers to forward declared classes.
ActionTypePickerWidget::~ActionTypePickerWidget()
{
}

QString ActionTypePickerWidget::actionType() const
{
    return ui->actionTypeComboBox->currentData().toString();
}

void ActionTypePickerWidget::setActionType(const QString& actionType)
{
    if (auto index = ui->actionTypeComboBox->findData(actionType); index != -1)
    {
        ui->actionTypeComboBox->setCurrentIndex(index);
        return;
    }

    ui->actionTypeComboBox->setPlaceholderText(
        vms::rules::utils::StringHelper{systemContext()}.actionName(actionType));
    ui->actionTypeComboBox->setCurrentIndex(-1);
}

} // namespace nx::vms::client::desktop::rules
