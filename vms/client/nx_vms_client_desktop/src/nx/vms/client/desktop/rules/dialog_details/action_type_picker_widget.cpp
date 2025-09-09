// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_type_picker_widget.h"
#include "ui_action_type_picker_widget.h"

#include <QtGui/QStandardItemModel>

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/watchers/cloud_service_checker.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/actions/push_notification_action.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/strings.h>
#include <nx/vms/rules/utils/action.h>
#include <nx/vms/rules/utils/compatibility.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/common/palette.h>

namespace {

const auto kPushNotificationActionId =
    nx::vms::rules::utils::type<nx::vms::rules::PushNotificationAction>();

} // namespace
namespace nx::vms::client::desktop::rules {

ActionTypePickerWidget::ActionTypePickerWidget(SystemContext* context, QWidget* parent):
    QWidget(parent),
    SystemContextAware{context},
    ui(new Ui::ActionTypePickerWidget())
{
    using namespace nx::vms::rules::utils;

    ui->setupUi(this);
    setPaletteColor(ui->doLabel, QPalette::WindowText, QPalette().color(QPalette::Light));

    auto itemFilters = defaultItemFilters();
    if (!appContext()->cloudServiceChecker()->hasService(
            nx::vms::client::core::CloudService::push_notifications))
    {
        const auto customFilter = [](const nx::vms::common::SystemContext* /*systemContext*/,
            const nx::vms::rules::ItemDescriptor& description)
        {
            return description.id != kPushNotificationActionId;
        };
        itemFilters.push_back(customFilter);
    }

    const auto sortedListOfActions = sortItems(filterItems(systemContext(),
        itemFilters,
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

void ActionTypePickerWidget::setProlongedActionsEnabled(bool enabled)
{
    const auto model = qobject_cast<QStandardItemModel*>(ui->actionTypeComboBox->model());
    if (!NX_ASSERT(model, "Unexpected model type is used by the QComboBox"))
        return;

    const auto engine = systemContext()->vmsRulesEngine();

    for (int row = 0; row < model->rowCount(); ++row)
    {
        auto item = model->item(row);
        const auto actionId = item->data(Qt::UserRole).toString();

        if (isProlongedOnly(*engine->actionDescriptor(actionId)))
        {
            item->setFlags(enabled
                ? item->flags() | Qt::ItemIsEnabled
                : item->flags() & ~Qt::ItemIsEnabled);
        }
    }
}

} // namespace nx::vms::client::desktop::rules
