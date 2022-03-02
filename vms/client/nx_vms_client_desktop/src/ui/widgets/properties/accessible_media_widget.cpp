// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "accessible_media_widget.h"

#include "ui_accessible_media_widget.h"

#include <nx/utils/log/assert.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <ui/models/abstract_permissions_model.h>

#include <nx/vms/client/desktop/resource_dialogs/details/accessible_media_view_header_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/accessible_media_dialog_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_selection_widget.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>

using namespace nx::vms::client::desktop;

namespace {

entity_item_model::AbstractEntityPtr createTreeEntity(
    const entity_resource_tree::ResourceTreeEntityBuilder* builder)
{
    return builder->createDialogShareableMediaEntity();
}

QSet<QnUuid> accessibleMediaResourcesIds(
    QnResourcePool* resourcePool,
    const QnUuidSet& sourceResourcesIds)
{
    return QnResourceAccessFilter::filteredResources(
        resourcePool,
        QnResourceAccessFilter::MediaFilter,
        sourceResourcesIds);
}

AccessibleMediaDialogItemDelegate* accessibleMediaItemDelegate(
    const ResourceSelectionWidget* resourceSelectionWidget)
{
    const auto result = dynamic_cast<AccessibleMediaDialogItemDelegate*>(
        resourceSelectionWidget->resourceViewWidget()->itemDelegate());

    NX_ASSERT(result, "Unexpected item delegate type");

    return result;
}

} // namespace

QnAccessibleMediaWidget::QnAccessibleMediaWidget(
    QnAbstractPermissionsModel* permissionsModel,
    QWidget* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::AccessibleMediaWidget()),
    m_permissionsModel(permissionsModel)
{
    NX_ASSERT(parent);

    ui->setupUi(this);

    m_resourceSelectionWidget = new ResourceSelectionWidget(
        ResourceSelectionWidget::ResourceValidator(),
        ResourceSelectionWidget::AlertTextProvider(),
        this);
    m_resourceSelectionWidget->setTreeEntityFactoryFunction(createTreeEntity);
    ui->widgetLayout->addWidget(m_resourceSelectionWidget);

    m_resourceSelectionWidget->resourceViewWidget()->setItemDelegate(
        new AccessibleMediaDialogItemDelegate(resourceAccessProvider(), this));

    m_headerWidget = new AccessibleMediaViewHeaderWidget();
    m_resourceSelectionWidget->resourceViewWidget()->setHeaderWidget(m_headerWidget);

    connect(m_headerWidget, &AccessibleMediaViewHeaderWidget::allMediaCheckableItemClicked, this,
        [this]
        {
            const bool newAllMediaCheckedValue = !m_headerWidget->allMediaCheked();

            m_headerWidget->setAllMediaCheked(newAllMediaCheckedValue);
            m_resourceSelectionWidget->resourceViewWidget()->setItemViewEnabled(
                !newAllMediaCheckedValue);

            emit hasChangesChanged();
        });

    m_resourceSelectionWidget->setDisplayResourceStatus(false);

    connect(m_resourceSelectionWidget, &ResourceSelectionWidget::selectionChanged,
        this, &QnAccessibleMediaWidget::hasChangesChanged);
}

QnAccessibleMediaWidget::~QnAccessibleMediaWidget()
{
}

bool QnAccessibleMediaWidget::hasChanges() const
{
    const auto permissionsChanged = m_permissionsModel->rawPermissions()
        .testFlag(GlobalPermission::accessAllMedia) != allCamerasItemChecked();

    const auto selectedResourcesChanged =
        [this]
        {
            const auto accessibleResourcesIds = m_permissionsModel->accessibleResources();
            return checkedResources()
                != accessibleMediaResourcesIds(resourcePool(), accessibleResourcesIds);
        };

    return permissionsChanged || selectedResourcesChanged();
}

void QnAccessibleMediaWidget::loadDataToUi()
{
    const auto accessibleResourcesIds = m_permissionsModel->accessibleResources();
    m_resourceSelectionWidget->setSelectedResourcesIds(
        accessibleMediaResourcesIds(resourcePool(), accessibleResourcesIds));

    if (const auto itemDelegate = accessibleMediaItemDelegate(m_resourceSelectionWidget))
        itemDelegate->setSubject(m_permissionsModel->subject());

    bool accessAllMedia =
        m_permissionsModel->rawPermissions().testFlag(GlobalPermission::accessAllMedia);

    // For custom users 'All Resources' must be unchecked by default.
    if (m_permissionsModel->subject().user())
    {
        accessAllMedia = accessAllMedia && m_permissionsModel->rawPermissions().testFlag(
            GlobalPermission::customUser);
    }

    m_headerWidget->setAllMediaCheked(accessAllMedia);
    m_resourceSelectionWidget->resourceViewWidget()->setItemViewEnabled(!accessAllMedia);
}

bool QnAccessibleMediaWidget::allCamerasItemChecked() const
{
    return m_headerWidget->allMediaCheked();
}

int QnAccessibleMediaWidget::resourcesCount() const
{
    return m_resourceSelectionWidget->resourceCount();
}

QSet<QnUuid> QnAccessibleMediaWidget::checkedResources() const
{
    return m_resourceSelectionWidget->selectedResourcesIds();
}

void QnAccessibleMediaWidget::applyChanges()
{
    auto accessibleResources = m_permissionsModel->accessibleResources();
    auto oldFiltered = QnResourceAccessFilter::filteredResources(
        resourcePool(),
        QnResourceAccessFilter::MediaFilter,
        accessibleResources);
    auto newFiltered = checkedResources();

    GlobalPermissions permissions = m_permissionsModel->rawPermissions();
    permissions.setFlag(GlobalPermission::accessAllMedia, allCamerasItemChecked());
    m_permissionsModel->setRawPermissions(permissions);

    if (!allCamerasItemChecked())
    {
        accessibleResources.subtract(oldFiltered);
        accessibleResources.unite(newFiltered);
        m_permissionsModel->setAccessibleResources(accessibleResources);
    }
}
