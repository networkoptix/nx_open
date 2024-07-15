// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_type_picker_widget.h"
#include "ui_action_type_picker_widget.h"

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/strings.h>
#include <nx/vms/rules/utils/common.h>
#include <nx/vms/rules/utils/compatibility.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop::rules {

ActionTypePickerWidget::ActionTypePickerWidget(SystemContext* context, QWidget* parent):
    QWidget(parent),
    SystemContextAware{context},
    ui(new Ui::ActionTypePickerWidget())
{
    using namespace nx::vms::rules::utils;

    ui->setupUi(this);
    setPaletteColor(ui->doLabel, QPalette::WindowText, QPalette().color(QPalette::Light));

    const auto sortedListOfActions = sortItems(filterItems(systemContext(),
        defaultItemFilters(),
        systemContext()->vmsRulesEngine()->actions().values()));
    for (const auto& actionDescriptor: sortedListOfActions)
        ui->actionTypeComboBox->addItem(actionDescriptor.displayName, actionDescriptor.id);

    connect(
        ui->actionTypeComboBox,
        &QComboBox::activated,
        this,
        [this]
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
        vms::rules::Strings::actionName(systemContext(), actionType));
    ui->actionTypeComboBox->setCurrentIndex(-1);
}

} // namespace nx::vms::client::desktop::rules
