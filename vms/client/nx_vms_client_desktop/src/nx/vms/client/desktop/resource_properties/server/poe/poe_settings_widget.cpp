#include "poe_settings_widget.h"

#include "poe_settings_store.h"

#include "ui_poe_settings_widget.h"

#include <nx/vms/client/desktop/resource_properties/server/poe/poe_controller.h>
#include <nx/vms/client/desktop/resource_properties/server/poe/poe_settings_reducer.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_helper.h>
#include <core/resource_management/resource_pool.h>

namespace nx::vms::client::desktop {

using namespace node_view;
using namespace node_view::details;

using PortMode = nx::vms::api::NetworkPortWithPoweringMode;
using Mode = PortMode::PoweringMode;

struct PoeSettingsWidget::Private: public QObject
{
    Private(PoeSettingsWidget* owner);

    void updateWarningLabel();
    void updatePreloaderVisibility();
    void updateUiBlock();

    void updateBlockData();
    void handlePatchApplied(const PoeSettingsStatePatch& patch);

    PoeSettingsWidget* const q;
    PoeController controller;
    Ui::PoeSettingsWidget ui;

    PoeSettingsStore store;
};

PoeSettingsWidget::Private::Private(PoeSettingsWidget* owner):
    q(owner),
    store(owner)
{
    ui.setupUi(owner);

    updateWarningLabel();
    updatePreloaderVisibility();
    updateUiBlock();

    store.setStores(ui.poeTable->store(), ui.totalsTable->store());
    connect(&store, &PoeSettingsStore::patchApplied, this, &Private::handlePatchApplied);

    connect(&controller, &PoeController::updated, this, &Private::updateBlockData);
    connect(ui.poeTable, &PoeSettingsTableView::hasUserChangesChanged, &store,
        [this]() { store.setHasChanges(ui.poeTable->hasUserChanges()); });
    connect(&controller, &PoeController::initialUpdateInProgressChanged, &store,
        [this]() { store.setPreloaderVisible(controller.initialUpdateInProgress()); });
    connect(&controller, &PoeController::updatingPoweringModesChanged, &store,
        [this]() { store.setBlockUi(controller.updatingPoweringModes()); });

    updateBlockData();
}

void PoeSettingsWidget::Private::updateWarningLabel()
{
    ui.poeOverBudgetWarningLabel->setVisible(store.state().showPoeOverBudgetWarning);
}

void PoeSettingsWidget::Private::updatePreloaderVisibility()
{
    ui.stackedWidget->setCurrentWidget(store.state().showPreloader
        ? ui.preloaderPage
        : ui.poeSettingsPage);
}

void PoeSettingsWidget::Private::updateUiBlock()
{
    const bool blockUi = store.state().blockUi;
    q->setReadOnly(blockUi);
    ui.poeTable->setEnabled(!blockUi);
    ui.totalsTable->setEnabled(!blockUi);
}

void PoeSettingsWidget::Private::updateBlockData()
{
    store.updateBlocks(controller.blockData());
}

void PoeSettingsWidget::Private::handlePatchApplied(const PoeSettingsStatePatch& patch)
{
    if (patch.hasChanges)
        emit q->hasChangesChanged();

    if (patch.showPoeOverBudgetWarning)
        updateWarningLabel();

    if (patch.showPreloader)
        updatePreloaderVisibility();

    if (patch.blockUi)
        updateUiBlock();

    if (patch.autoUpdates)
        controller.setAutoUpdate(patch.autoUpdates.value());
}

//--------------------------------------------------------------------------------------------------

PoeSettingsWidget::PoeSettingsWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
{
}

PoeSettingsWidget::~PoeSettingsWidget()
{
}

bool PoeSettingsWidget::hasChanges() const
{
    return d->store.state().hasChanges;
}

void PoeSettingsWidget::loadDataToUi()
{
}

void PoeSettingsWidget::applyChanges()
{
    PoeController::PowerModes modes;
    const auto checkedIndices = d->ui.poeTable->userCheckChangedIndices();
    for (const auto index: checkedIndices)
    {
        PortMode mode;
        mode.poweringMode = ViewNodeHelper(index).userCheckedState(index.column()) == Qt::Checked
            ? Mode::automatic
            : Mode::off;
        mode.portNumber = ViewNodeHelper::nodeFromIndex(index)->property(
            PoeSettingsReducer::kPortNumberProperty).toInt();
        modes.append(mode);
    }

    d->store.applyUserChanges();
    d->controller.setPowerModes(modes);
}

void PoeSettingsWidget::setServerId(const QnUuid& value)
{
    d->controller.setResourceId(value);
}

void PoeSettingsWidget::setAutoUpdate(bool value)
{
    d->store.setAutoUpdates(value);
}

} // namespace nx::vms::client::desktop
