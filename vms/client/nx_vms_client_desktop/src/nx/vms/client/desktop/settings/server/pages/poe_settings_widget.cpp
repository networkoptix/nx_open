#include "poe_settings_widget.h"

#include "poe_settings_store.h"

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
    void handlePatchApplied(const PoESettingsStatePatch& patch);

    PoESettingsWidget* const q;
    Controller controller;
    Ui::PoESettingsWidget ui;

    PoESettingsStore store;
};

PoESettingsWidget::Private::Private(PoESettingsWidget* owner):
    q(owner),
    store(owner)
{
    ui.setupUi(owner);

    store.setStores(ui.poeTable->store(), ui.poeTable->store() /*<tmp*/);
    connect(&store, &PoESettingsStore::patchApplied, this, &Private::handlePatchApplied);

    connect(&controller, &Controller::updated, this, &Private::updateBlockData);
    connect(ui.poeTable, &PoESettingsTableView::hasUserChangesChanged, &store,
        [this]() { store.setHasChanges(ui.poeTable->hasUserChanges()); });
    connect(&controller, &Controller::updatingPoweringModesChanged, &store,
        [this]() { store.setBlockUi(controller.updatingPoweringModes()); });

    updateBlockData();
    ui.poeOverBudgetWarningLabel->setVisible(false);
}

void PoESettingsWidget::Private::updateBlockData()
{
    store.updateBlocks(controller.blockData());
}

void PoESettingsWidget::Private::handlePatchApplied(const PoESettingsStatePatch& patch)
{
    if (patch.hasChanges)
        emit q->hasChangesChanged();

    if (patch.showPoEOverBudgetWarning)
        ui.poeOverBudgetWarningLabel->setVisible(patch.showPoEOverBudgetWarning.value());

    if (patch.blockUi)
    {
        const bool blockUi = patch.blockUi.value();
        q->setReadOnly(blockUi);
        ui.poeTable->setEnabled(!blockUi);
        ui.poeTotalsTable->setEnabled(!blockUi);
    }
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
    return d->store.state().hasChanges;
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

    d->store.applyUserChanges();
    d->controller.setPowerModes(modes);
}

void PoESettingsWidget::setServerId(const QnUuid& value)
{
    d->controller.setResourceId(value);
}

} // namespace settings
} // namespace nx::vms::client::desktop
