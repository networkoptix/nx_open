#include "user_access_rights_resources_widget.h"
#include "ui_user_access_rights_resources_widget.h"

#include <client/client_globals.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>

#include <ui/models/resource_list_model.h>

QnUserAccessRightsResourcesWidget::QnUserAccessRightsResourcesWidget(Filter filter, QWidget* parent /*= 0*/):
    base_type(parent),
    ui(new Ui::UserAccessRightsResourcesWidget()),
    m_filter(filter),
    m_targetId(),
    m_model(new QnResourceListModel())
{
    ui->setupUi(this);
    ui->descriptionLabel->setText(tr("Giving access to some layouts you give access to all cameras on them. Also user will get access to all new cameras on this layouts."));

    m_model->setCheckable(true);
    m_model->setResources(qnResPool->getAllCameras(QnResourcePtr(), true));
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

QnUserAccessRightsResourcesWidget::~QnUserAccessRightsResourcesWidget()
{

}


QnUuid QnUserAccessRightsResourcesWidget::targetId() const
{
    return m_targetId;
}

void QnUserAccessRightsResourcesWidget::setTargetId(const QnUuid& id)
{
    NX_ASSERT(targetIsValid(id));
    m_targetId = id;
}

bool QnUserAccessRightsResourcesWidget::hasChanges() const
{
    /* Validate target. */
    if (!targetIsValid(m_targetId))
        return false;

    auto accessibleResources = qnResourceAccessManager->accessibleResources(m_targetId);
    //TODO: #GDM #access filter by type

    return m_model->checkedResources() != accessibleResources;
}

void QnUserAccessRightsResourcesWidget::loadDataToUi()
{
    /* Validate target. */
    if (!targetIsValid(m_targetId))
        return;

    auto accessibleResources = qnResourceAccessManager->accessibleResources(m_targetId);
    //TODO: #GDM #access filter by type

    m_model->setCheckedResources(accessibleResources);
}

void QnUserAccessRightsResourcesWidget::applyChanges()
{
    /* Validate target. */
    if (!targetIsValid(m_targetId))
        return;

    //TODO: #GDM #access filter by type
    qnResourceAccessManager->setAccessibleResources(m_targetId, m_model->checkedResources());
}

bool QnUserAccessRightsResourcesWidget::targetIsValid(const QnUuid& id) const
{
    if (id.isNull())
        return false;

    /* Check if it is valid user id. */
    if (qnResPool->getResourceById<QnUserResource>(id))
        return true;

    /* Check if it is valid user group id. */
    const auto& userGroups = qnResourceAccessManager->userGroups();
    return boost::algorithm::any_of(userGroups, [id](const ec2::ApiUserGroupData& group) { return group.id == id; });
}

