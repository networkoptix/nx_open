#include "accessible_resources_widget.h"
#include "ui_accessible_resources_widget.h"

#include <client/client_globals.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource_type.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_camera.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>


#include <ui/delegates/resource_item_delegate.h>
#include <ui/models/resource_list_model.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/string.h>

namespace
{
    class QnResourcesListSortModel: public QSortFilterProxyModel
    {
        typedef QSortFilterProxyModel base_type;
    public:
        explicit QnResourcesListSortModel(QObject *parent = 0) :
            base_type(parent)
        {
        }
        virtual ~QnResourcesListSortModel() {}

        QnResourcePtr resource(const QModelIndex &index) const
        {
            return index.data(Qn::ResourceRole).value<QnResourcePtr>();
        }

    protected:
        virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
        {
            QnResourcePtr l = resource(left);
            QnResourcePtr r = resource(right);

            if (!l || !r)
                return l < r;

            if (l->getTypeId() != r->getTypeId())
            {
                QnVirtualCameraResourcePtr lcam = l.dynamicCast<QnVirtualCameraResource>();
                QnVirtualCameraResourcePtr rcam = r.dynamicCast<QnVirtualCameraResource>();
                if (!lcam || !rcam)
                    return lcam < rcam;
            }

            {
                /* Sort by name. */
                QString leftDisplay = left.data(Qt::DisplayRole).toString();
                QString rightDisplay = right.data(Qt::DisplayRole).toString();
                int result = naturalStringCompare(leftDisplay, rightDisplay, Qt::CaseInsensitive);
                if (result != 0)
                    return result < 0;
            }

            /* We want the order to be defined even for items with the same name. */
            return l->getUniqueId() < r->getUniqueId();
        }

    };

    const QString kDummyResourceId(lit("dummy_resource"));
}

QnAccessibleResourcesWidget::QnAccessibleResourcesWidget(QnAbstractPermissionsModel* permissionsModel, QnResourceAccessFilter::Filter filter, QWidget* parent /*= 0*/):
    base_type(parent),
    ui(new Ui::AccessibleResourcesWidget()),
    m_permissionsModel(permissionsModel),
    m_filter(filter),
    m_controlsVisible(filter == QnResourceAccessFilter::CamerasFilter), /*< Show 'All' checkbox only for cameras. */
    m_resourcesModel(new QnResourceListModel()),
    m_controlsModel(new QnResourceListModel())
{
    ui->setupUi(this);

    switch (m_filter)
    {
    case QnResourceAccessFilter::LayoutsFilter:
        ui->descriptionLabel->setText(tr("Giving access to some layouts you give access to all cameras on them. Also user will get access to all new cameras on these layouts."));
        break;
    default:
        break;
    }

    initControlsModel();
    initResourcesModel();

    QnResourcesListSortModel* viewModel = new QnResourcesListSortModel(this);
    viewModel->setSourceModel(m_resourcesModel.data());
    viewModel->setDynamicSortFilter(true);
    viewModel->setSortRole(Qt::DisplayRole);
    viewModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    viewModel->sort(Qn::NameColumn);

    ui->resourcesTreeView->setModel(viewModel);
    ui->controlsTreeView->setModel(m_controlsModel.data());
    ui->controlsTreeView->setVisible(m_controlsVisible);
    ui->controlsTreeView->setEnabled(m_controlsVisible);
    ui->line->setVisible(m_controlsVisible);

    auto itemDelegate = new QnResourceItemDelegate(this);

    auto setupTreeView = [itemDelegate](QnTreeView* treeView)
    {
        treeView->setItemDelegate(itemDelegate);
        treeView->header()->setStretchLastSection(false);
        treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        treeView->header()->setSectionResizeMode(QnResourceListModel::NameColumn, QHeaderView::Stretch);
    };
    setupTreeView(ui->resourcesTreeView);
    setupTreeView(ui->controlsTreeView);

    ui->resourcesTreeView->setMouseTracking(true);

    connect(ui->resourcesTreeView, &QAbstractItemView::entered, this, &QnAccessibleResourcesWidget::updateThumbnail);
    connect(ui->resourcesTreeView, &QnTreeView::spacePressed, this, [this, viewModel](const QModelIndex& index)
    {
        QModelIndex checkedIdx = index.sibling(index.row(), QnResourceListModel::CheckColumn);
        bool checked = checkedIdx.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        int inverted = checked ? Qt::Unchecked : Qt::Checked;
        viewModel->setData(checkedIdx, inverted, Qt::CheckStateRole);
    });

    updateThumbnail();
}

QnAccessibleResourcesWidget::~QnAccessibleResourcesWidget()
{

}

bool QnAccessibleResourcesWidget::hasChanges() const
{
    if (m_controlsVisible)
    {
        bool checkedAll = !m_controlsModel->checkedResources().isEmpty();
        if (m_permissionsModel->rawPermissions().testFlag(Qn::GlobalAccessAllCamerasPermission) != checkedAll)
            return true;
    }

    return m_resourcesModel->checkedResources() != QnResourceAccessFilter::filteredResources(m_filter, m_permissionsModel->accessibleResources());
}

void QnAccessibleResourcesWidget::loadDataToUi()
{
    if (m_controlsVisible)
    {
        QSet<QnUuid> checkedControls;
        if (m_permissionsModel->rawPermissions().testFlag(Qn::GlobalAccessAllCamerasPermission))
            for (const auto& resource : m_controlsModel->resources())
                checkedControls << resource->getId();   /*< Really we are checking the only dummy resource. */
        m_controlsModel->setCheckedResources(checkedControls);
    }
    m_resourcesModel->setCheckedResources(QnResourceAccessFilter::filteredResources(m_filter, m_permissionsModel->accessibleResources()));
}

void QnAccessibleResourcesWidget::applyChanges()
{
    auto accessibleResources = m_permissionsModel->accessibleResources();
    auto oldFiltered = QnResourceAccessFilter::filteredResources(m_filter, accessibleResources);
    auto newFiltered = m_resourcesModel->checkedResources();
    accessibleResources.subtract(oldFiltered);
    accessibleResources.unite(newFiltered);
    m_permissionsModel->setAccessibleResources(accessibleResources);

    if (m_controlsVisible)
    {
        bool checkedAll = !m_controlsModel->checkedResources().isEmpty();
        Qn::GlobalPermissions permissions = m_permissionsModel->rawPermissions();
        if (checkedAll)
            permissions |= Qn::GlobalAccessAllCamerasPermission;
        else
            permissions &= ~Qn::GlobalAccessAllCamerasPermission;
        m_permissionsModel->setRawPermissions(permissions);
    }
}

void QnAccessibleResourcesWidget::initControlsModel()
{
    if (!m_controlsVisible)
        return;

    QnVirtualCameraResourcePtr dummy(new QnClientCameraResource(qnResTypePool->getFixedResourceTypeId(kDummyResourceId)));
    dummy->setName(tr("All Cameras"));
    /* Create separate dummy resource id for each filter, but once per application run. */
    dummy->setId(QnUuid::createUuidFromPool(guidFromArbitraryData(kDummyResourceId).getQUuid(), m_filter));
    m_controlsModel->setResources(QnResourceList() << dummy);
    m_controlsModel->setCheckable(true);
    m_controlsModel->setStatusIgnored(true);

    connect(m_controlsModel.data(), &QnResourceListModel::dataChanged, this, [this](const QModelIndex& index)
    {
        QModelIndex checkedIdx = index.sibling(index.row(), QnResourceListModel::CheckColumn);
        bool checked = checkedIdx.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        ui->resourcesTreeView->setEnabled(!checked);
        emit hasChangesChanged();
    });
}

void QnAccessibleResourcesWidget::initResourcesModel()
{
    m_resourcesModel->setCheckable(true);
    m_resourcesModel->setStatusIgnored(true);
    m_resourcesModel->setResources(QnResourceAccessFilter::filteredResources(m_filter, qnResPool->getResources()));

    connect(m_resourcesModel.data(), &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
    {
        if (roles.contains(Qt::CheckStateRole)
            && topLeft.column() <= QnResourceListModel::CheckColumn
            && bottomRight.column() >= QnResourceListModel::CheckColumn
            )
            emit hasChangesChanged();
    });
}

void QnAccessibleResourcesWidget::updateThumbnail(const QModelIndex& index)
{
    if (!index.isValid())
    {
        ui->nameLabel->setText(QString());
        ui->previewWidget->hide();
        return;
    }

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
}

