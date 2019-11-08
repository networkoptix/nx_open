#include "poe_settings_widget.h"

#include "ui_poe_settings_widget.h"

#include <nx/vms/client/core/poe_settings/poe_controller.h>
#include <nx/vms/client/desktop/settings/server/pages/poe_settings_reducer.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_helpers.h>
#include <core/resource_management/resource_pool.h>

namespace nx::vms::client::desktop {
namespace settings {

using namespace node_view;
using namespace node_view::details;

using Controller = core::PoEController;
using PortMode = nx::vms::api::NetworkPortWithPoweringMode;
using Mode = PortMode::PoweringMode;


struct PoESettingsWidget::Private: public QObject
{
    Private(PoESettingsWidget* owner);
    void updateBlockData();

    PoESettingsWidget* const q;
    Controller controller;
    Ui::PoESettingsWidget ui;
};

PoESettingsWidget::Private::Private(PoESettingsWidget* owner):
    q(owner)
{
    ui.setupUi(owner);

    connect(&controller, &Controller::updated, this, &Private::updateBlockData);
    connect(ui.poeTable, &PoESettingsTableView::hasUserChangesChanged,
        q, &PoESettingsWidget::hasChangesChanged);
    updateBlockData();
}

void PoESettingsWidget::Private::updateBlockData()
{
    const auto table = ui.poeTable;
    const auto& optionalBlockData = controller.blockData();
    const auto& blockData = optionalBlockData
        ? optionalBlockData.value()
        : nx::vms::api::NetworkBlockData();

    const auto patch = PoESettingsReducer::applyBlockDataChanges(
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
    return d->ui.poeTable->hasUserChanges();
}

void PoESettingsWidget::loadDataToUi()
{
}

void PoESettingsWidget::applyChanges()
{
    Controller::PowerModes modes;
    const auto checkedIndices = d->ui.poeTable->userCheckChangedIndices();
    for (const auto index: checkedIndices)
    {
        PortMode mode;
        mode.poweringMode = userCheckedState(index) == Qt::Checked ? Mode::automatic : Mode::off;
        mode.portNumber = nodeFromIndex(index)->property(PoESettingsReducer::kPortNumberProperty).toInt();
        modes.append(mode);
    }

    d->ui.poeTable->applyUserChanges();
    d->controller.setPowerModes(modes);
}

void PoESettingsWidget::setServerId(const QnUuid& value)
{
    d->controller.setResourceId(value);
}

} // namespace settings
} // namespace nx::vms::client::desktop
