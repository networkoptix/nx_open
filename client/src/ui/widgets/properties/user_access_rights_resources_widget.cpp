#include "user_access_rights_resources_widget.h"
#include "ui_user_access_rights_resources_widget.h"

#include <client/client_globals.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <ui/models/resource_list_model.h>

QnUserAccessRightsResourcesWidget::QnUserAccessRightsResourcesWidget(QWidget* parent /*= 0*/):
    base_type(parent),
    ui(new Ui::UserAccessRightsResourcesWidget()),
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

QSet<QnUuid> QnUserAccessRightsResourcesWidget::checkedResources() const
{
    return m_checkedResources;
}

void QnUserAccessRightsResourcesWidget::setCheckedResources(const QSet<QnUuid>& value)
{
    m_checkedResources = value;
}

bool QnUserAccessRightsResourcesWidget::hasChanges() const
{
    return m_model->checkedResources() != m_checkedResources;
}

void QnUserAccessRightsResourcesWidget::loadDataToUi()
{
    m_model->setCheckedResources(m_checkedResources);
}

void QnUserAccessRightsResourcesWidget::applyChanges()
{
    m_checkedResources = m_model->checkedResources();
}
