#include "accessible_resources_widget.h"
#include "ui_accessible_resources_widget.h"

#include <client/client_globals.h>

#include <core/resource_management/resource_pool.h>


#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>

#include <ui/delegates/abstract_permissions_delegate.h>
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

QnAccessibleResourcesWidget::QnAccessibleResourcesWidget(QnAbstractPermissionsDelegate* delegate, Filter filter, QWidget* parent /*= 0*/):
    base_type(parent),
    ui(new Ui::AccessibleResourcesWidget()),
    m_delegate(delegate),
    m_filter(filter),
    m_model(new QnResourceListModel())
{
    ui->setupUi(this);

    switch (m_filter)
    {
    case CamerasFilter:
        ui->allResourcesCheckBox->setText(tr("All Cameras"));
        break;
    case LayoutsFilter:
        ui->allResourcesCheckBox->setText(tr("All Global Layouts"));
        ui->descriptionLabel->setText(tr("Giving access to some layouts you give access to all cameras on them. Also user will get access to all new cameras on these layouts."));
        break;
    case ServersFilter:
        ui->allResourcesCheckBox->setText(tr("All Servers"));
        ui->descriptionLabel->setText(tr("Giving access to some server you give access to view server statistics."));
        break;
    default:
        break;
    }

    m_model->setCheckable(true);
    m_model->setResources(filteredResources(m_filter, qnResPool->getResources()));
    connect(m_model.data(), &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
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
    connect(ui->allResourcesCheckBox, &QCheckBox::clicked, this, [this]
    {
        ui->resourcesTreeView->setEnabled(!ui->allResourcesCheckBox->isChecked());
        emit hasChangesChanged();
    });

    updateThumbnail(QModelIndex());
}

QnAccessibleResourcesWidget::~QnAccessibleResourcesWidget()
{

}

bool QnAccessibleResourcesWidget::hasChanges() const
{
    if (m_delegate->permissions().testFlag(allResourcesPermission()) != ui->allResourcesCheckBox->isChecked())
        return true;

    return m_model->checkedResources() != filteredResources(m_filter, m_delegate->accessibleResources());
}

void QnAccessibleResourcesWidget::loadDataToUi()
{
    ui->allResourcesCheckBox->setChecked(m_delegate->permissions().testFlag(allResourcesPermission()));
    m_model->setCheckedResources(filteredResources(m_filter, m_delegate->accessibleResources()));
}

void QnAccessibleResourcesWidget::applyChanges()
{
    auto accessibleResources = m_delegate->accessibleResources();
    auto oldFiltered = filteredResources(m_filter, accessibleResources);
    auto newFiltered = m_model->checkedResources();
    accessibleResources.subtract(oldFiltered);
    accessibleResources.unite(newFiltered);
    m_delegate->setAccessibleResources(accessibleResources);

    Qn::GlobalPermissions permissions = m_delegate->permissions();
    if (ui->allResourcesCheckBox->isChecked())
        permissions |= allResourcesPermission();
    else
        permissions &= ~allResourcesPermission();
    m_delegate->setPermissions(permissions);
}

Qn::GlobalPermission QnAccessibleResourcesWidget::allResourcesPermission() const
{
    switch (m_filter)
    {
    case CamerasFilter:
        return Qn::GlobalAccessAllCamerasPermission;
    case LayoutsFilter:
        return Qn::GlobalAccessAllLayoutsPermission;
    case ServersFilter:
        return Qn::GlobalAccessAllServersPermission;

    default:
        break;
    }
    NX_ASSERT(false);
    return Qn::NoGlobalPermissions;
}

//
//bool QnAccessibleResourcesWidget::targetIsValid() const
//{
//    /* Check if it is valid user id and we have access rights to edit it. */
//    if (m_targetUser)
//    {
//        Qn::Permissions permissions = accessController()->permissions(m_targetUser);
//        return permissions.testFlag(Qn::WriteAccessRightsPermission);
//    }
//
//    if (m_targetGroupId.isNull())
//        return false;
//
//    /* Only admins can edit groups. */
//    if (!accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
//        return false;
//
//    /* Check if it is valid user group id. */
//    const auto& userGroups = qnResourceAccessManager->userGroups();
//    return boost::algorithm::any_of(userGroups, [this](const ec2::ApiUserGroupData& group) { return group.id == m_targetGroupId; });
//}

