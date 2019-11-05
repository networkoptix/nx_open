#include "poe_settings_widget.h"

#include "ui_poe_settings_widget.h"

#include <nx/vms/client/core/poe_settings/poe_controller.h>
#include <nx/vms/client/desktop/settings/server/pages/poe_settings_table_state_reducer.h>
#include <core/resource_management/resource_pool.h>

namespace nx::vms::client::desktop {
namespace settings {

struct PoESettingsWidget::Private: public QObject
{
    Private(PoESettingsWidget* owner);
    void updateBlockData();

    PoESettingsWidget* const q;
    core::PoEController controller;
    Ui::PoESettingsWidget ui;
};

PoESettingsWidget::Private::Private(PoESettingsWidget* owner):
    q(owner)
{
    ui.setupUi(owner);

    connect(&controller, &core::PoEController::updated, this, &Private::updateBlockData);
    updateBlockData();
}

void PoESettingsWidget::Private::updateBlockData()
{
    const auto table = ui.poeTable;
    const auto& optionalBlockData = controller.blockData();
    const auto& blockData = optionalBlockData
        ? optionalBlockData.value()
        : nx::vms::api::NetworkBlockData();

    const auto patch = PoESettingsTableStateReducer::applyBlockDataChanges(
        table->state(), blockData, *q->resourcePool());
    table->applyPatch(patch);
}

//--------------------------------------------------------------------------------------------------

PoESettingsWidget::PoESettingsWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
{
}

PoESettingsWidget::~PoESettingsWidget()
{
}

bool PoESettingsWidget::hasChanges() const
{
    return false;
}

void PoESettingsWidget::loadDataToUi()
{
}

void PoESettingsWidget::applyChanges()
{
}

void PoESettingsWidget::setServerId(const QnUuid& value)
{
    d->controller.setResourceId(value);
}

} // namespace settings
} // namespace nx::vms::client::desktop
