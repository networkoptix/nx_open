// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "poe_settings_widget.h"
#include "ui_poe_settings_widget.h"

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_helper.h>
#include <nx/vms/client/desktop/resource_properties/server/poe/poe_controller.h>
#include <nx/vms/client/desktop/resource_properties/server/poe/poe_settings_reducer.h>

#include "poe_settings_store.h"

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
    store(owner->resourcePool())
{
    ui.setupUi(owner);
    ui.poeOverBudgetWarningLabel->init(
        {.text = tr("Attention! Power limit exceeded"), .level = BarDescription::BarLevel::Error});

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
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
}

bool PoeSettingsWidget::hasChanges() const
{
    return d->store.state().hasChanges;
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

void PoeSettingsWidget::discardChanges()
{
    d->controller.cancelRequest();
}

bool PoeSettingsWidget::isNetworkRequestRunning() const
{
    return d->controller.isNetworkRequestRunning();
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
