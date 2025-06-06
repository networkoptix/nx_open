// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_selection_dialog.h"
#include "ui_camera_selection_dialog.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QPushButton>

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/models/resource_selection_decorator_model.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/generic_item/generic_item_builder.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/desktop/resource_views/resource_tree_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

CameraSelectionDialog::CameraSelectionDialog(
    const ResourceFilter& resourceFilter,
    const ResourceValidator& resourceValidator,
    const AlertTextProvider& alertTextProvider,
    bool permissionsHandledByFilter,
    const std::optional<PinnedItemDescription>& pinnedItemDescription,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::CameraSelectionDialog())
{
    ui->setupUi(this);

    ResourceSelectionWidget::EntityFactoryFunction createTreeEntity =
        [=, this](const entity_resource_tree::ResourceTreeEntityBuilder* builder)
        {
            using namespace entity_item_model;

            auto camerasEntity = builder->createDialogAllCamerasEntity(
                showServersInTree(), resourceFilter, permissionsHandledByFilter);

            if (!pinnedItemDescription)
                return camerasEntity;

            AbstractItemPtr pinnedItem = GenericItemBuilder()
                .withRole(Qn::ResourceIconKeyRole, static_cast<int>(pinnedItemDescription->iconKey))
                .withRole(Qt::DisplayRole, pinnedItemDescription->displayText)
                .withRole(ResourceDialogItemRole::IsPinnedItemRole, true)
                .withFlags({Qt::ItemIsEnabled, Qt::ItemNeverHasChildren});

            return builder->addPinnedItem(
                std::move(camerasEntity),
                std::move(pinnedItem),
                /*withSeparator*/ false);
        };

    m_resourceSelectionWidget = new ResourceSelectionWidget(
        systemContext(), this, resource_selection_view::ColumnCount, resourceValidator);
    m_resourceSelectionWidget->setAlertTextProvider(alertTextProvider);
    m_resourceSelectionWidget->setTreeEntityFactoryFunction(createTreeEntity);
    ui->widgetLayout->insertWidget(0, m_resourceSelectionWidget);

    const auto allCamerasSwitch = ui->allCamerasSwitch;

    allCamerasSwitch->setVisible(true);
    allCamerasSwitch->setText(
        QnDeviceDependentStrings::getDefaultNameFromSet(system()->resourcePool(),
            tr("Show all devices"),
            tr("Show all cameras")));

    const auto updateDialogTitle =
        [this](ResourceSelectionMode mode)
        {
            if (mode == ResourceSelectionMode::MultiSelection)
            {
                setWindowTitle(QnDeviceDependentStrings::getDefaultNameFromSet(
                    system()->resourcePool(), tr("Select Devices"), tr("Select Cameras")));
            }
            else
            {
                setWindowTitle(QnDeviceDependentStrings::getDefaultNameFromSet(
                    system()->resourcePool(), tr("Select Device"), tr("Select Camera")));
            }
        };

    QObject::connect(resourceSelectionWidget(), &ResourceSelectionWidget::selectionModeChanged,
        updateDialogTitle);
    updateDialogTitle(resourceSelectionWidget()->selectionMode());

    connect(resourceSelectionWidget(), &ResourceSelectionWidget::selectionChanged,
        this, &CameraSelectionDialog::updateDialogControls);

    connect(ui->allCamerasSwitch, &QPushButton::toggled,
        m_resourceSelectionWidget, &ResourceSelectionWidget::setShowInvalidResources);
}

CameraSelectionDialog::~CameraSelectionDialog()
{
}

ResourceSelectionWidget* CameraSelectionDialog::resourceSelectionWidget() const
{
    return m_resourceSelectionWidget;
}

bool CameraSelectionDialog::allCamerasSwitchVisible() const
{
    return !ui->allCamerasSwitch->isHidden();
}

void CameraSelectionDialog::setAllCamerasSwitchVisible(bool visible)
{
    ui->allCamerasSwitch->setHidden(!visible);
}

bool CameraSelectionDialog::allowInvalidSelection() const
{
    return m_allowInvalidSelection;
}

void CameraSelectionDialog::setAllowInvalidSelection(bool value)
{
    if (m_allowInvalidSelection == value)
        return;

    m_allowInvalidSelection = value;

    updateDialogControls();
}

bool CameraSelectionDialog::showServersInTree() const
{
    const auto isPowerUser = systemContext()->accessController()->hasPowerUserPermissions();

    const bool currentUserAllowedToShowServers =
        isPowerUser || systemContext()->globalSettings()->showServersInTreeForNonAdmins();

    return currentUserAllowedToShowServers
        && workbenchContext()->resourceTreeSettings()->showServersInTree();
}

void CameraSelectionDialog::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);
    updateDialogControls();
    m_resourceSelectionWidget->resourceViewWidget()->makeRequiredItemsVisible();

}

void CameraSelectionDialog::updateDialogControls()
{
    const auto selectionHasInvalidResources =
        m_resourceSelectionWidget->selectionHasInvalidResources();

    ui->allCamerasSwitch->setEnabled(!selectionHasInvalidResources);

    if (buttonBox())
    {
        if (auto okButton = buttonBox()->button(QDialogButtonBox::Ok))
            okButton->setEnabled(!selectionHasInvalidResources || m_allowInvalidSelection);
    }
}

QString CameraSelectionDialog::noCamerasAvailableMessage()
{
    return tr("No cameras available");
}

QString CameraSelectionDialog::sourceCamera()
{
    return tr("Source camera");
}

} // namespace nx::vms::client::desktop
