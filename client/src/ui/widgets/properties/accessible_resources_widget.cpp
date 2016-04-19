#include "accessible_resources_widget.h"
#include "ui_accessible_resources_widget.h"

#include <client/client_globals.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>

#include <ui/models/resource_list_model.h>
#include <ui/workbench/workbench_access_controller.h>

namespace
{
    QnResourceList filteredResources(QnAccessibleResourcesWidget::Filter filter, const QnResourceList& source)
    {
        switch (filter)
        {
        case QnAccessibleResourcesWidget::CamerasFilter:
            return source.filtered<QnResource>([](const QnResourcePtr& resource)
            {
                if (resource->hasFlags(Qn::desktop_camera))
                    return false;
                return (resource->flags().testFlag(Qn::web_page) || resource->flags().testFlag(Qn::server_live_cam));
            });

        case QnAccessibleResourcesWidget::LayoutsFilter:
            return source.filtered<QnLayoutResource>([](const QnLayoutResourcePtr& layout)
            {
                return layout->isGlobal() && !layout->isFile();
            });

        case QnAccessibleResourcesWidget::ServersFilter:
            return source.filtered<QnMediaServerResource>([](const QnMediaServerResourcePtr& server)
            {
                return !QnMediaServerResource::isFakeServer(server);
            });
        }
        return QnResourceList();
    }

    QSet<QnUuid> filteredResources(QnAccessibleResourcesWidget::Filter filter, const QSet<QnUuid>& source)
    {
        QSet<QnUuid> result;
        for (const auto& resource : filteredResources(filter, qnResPool->getResources(source)))
            result << resource->getId();
        return result;
    }

}

QnAccessibleResourcesWidget::QnAccessibleResourcesWidget(Filter filter, QWidget* parent /*= 0*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::AccessibleResourcesWidget()),
    m_filter(filter),
    m_targetGroupId(),
    m_targetUser(),
    m_model(new QnResourceListModel())
{
    ui->setupUi(this);
    ui->descriptionLabel->setText(tr("Giving access to some layouts you give access to all cameras on them. Also user will get access to all new cameras on this layouts."));

    m_model->setCheckable(true);
    m_model->setResources(filteredResources(m_filter, qnResPool->getResources()));
    connect(m_model, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
    {
        if (roles.contains(Qt::CheckStateRole)
            && topLeft.column() <= QnResourceListModel::CheckColumn
            && bottomRight.column() >= QnResourceListModel::CheckColumn
            )
            emit hasChangesChanged();
    });

    ui->resourcesTreeView->setModel(m_model.data());
    ui->resourcesTreeView->setMouseTracking(true);

    auto updateThumbnail = [this](const QModelIndex& index)
    {
        QModelIndex baseIndex = index.column() == QnResourceListModel::NameColumn
            ? index
            : index.sibling(index.row(), QnResourceListModel::NameColumn);

        QString toolTip = baseIndex.data(Qt::ToolTipRole).toString();
        ui->nameLabel->setText(toolTip);

        QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        if (QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
        {
            ui->previewWidget->setTargetResource(camera->getId());
            ui->previewWidget->show();
        }
        else
        {
            ui->previewWidget->hide();
        }
    };

    connect(ui->resourcesTreeView, &QAbstractItemView::entered, this, updateThumbnail);
    updateThumbnail(QModelIndex());
}

QnAccessibleResourcesWidget::~QnAccessibleResourcesWidget()
{

}


QnUuid QnAccessibleResourcesWidget::targetGroupId() const
{
    return m_targetGroupId;
}

void QnAccessibleResourcesWidget::setTargetGroupId(const QnUuid& id)
{
    NX_ASSERT(m_targetUser.isNull());
    m_targetGroupId = id;
    NX_ASSERT(m_targetGroupId.isNull() || targetIsValid());
}

QnUserResourcePtr QnAccessibleResourcesWidget::targetUser() const
{
    return m_targetUser;
}

void QnAccessibleResourcesWidget::setTargetUser(const QnUserResourcePtr& user)
{
    NX_ASSERT(m_targetGroupId.isNull());
    m_targetUser = user;
    NX_ASSERT(!user || targetIsValid());
}

bool QnAccessibleResourcesWidget::hasChanges() const
{
    /* Validate target. */
    if (!targetIsValid())
        return false;

    auto accessibleResources = qnResourceAccessManager->accessibleResources(targetId());
    return m_model->checkedResources() != filteredResources(m_filter, accessibleResources);
}

void QnAccessibleResourcesWidget::loadDataToUi()
{
    /* Validate target. */
    if (!targetIsValid())
        return;

    auto accessibleResources = qnResourceAccessManager->accessibleResources(targetId());
    m_model->setCheckedResources(filteredResources(m_filter, accessibleResources));
}

void QnAccessibleResourcesWidget::applyChanges()
{
    /* Validate target. */
    if (!targetIsValid())
        return;

    auto accessibleResources = qnResourceAccessManager->accessibleResources(targetId());
    auto oldFiltered = filteredResources(m_filter, accessibleResources);
    auto newFiltered = m_model->checkedResources();
    accessibleResources.subtract(oldFiltered);
    accessibleResources.unite(newFiltered);
    qnResourceAccessManager->setAccessibleResources(targetId(), accessibleResources);
}

bool QnAccessibleResourcesWidget::targetIsValid() const
{
    /* Check if it is valid user id and we have access rights to edit it. */
    if (m_targetUser)
    {
        Qn::Permissions permissions = accessController()->permissions(m_targetUser);
        return permissions.testFlag(Qn::WriteAccessRightsPermission);
    }

    if (m_targetGroupId.isNull())
        return false;

    /* Only admins can edit groups. */
    if (!accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
        return false;

    /* Check if it is valid user group id. */
    const auto& userGroups = qnResourceAccessManager->userGroups();
    return boost::algorithm::any_of(userGroups, [this](const ec2::ApiUserGroupData& group) { return group.id == m_targetGroupId; });
}

QnUuid QnAccessibleResourcesWidget::targetId() const
{
    return m_targetUser
        ? m_targetUser->getId()
        : m_targetGroupId;
}

