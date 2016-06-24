#include "accessible_resources_widget.h"
#include "ui_accessible_resources_widget.h"

#include <client/client_globals.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource_type.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_camera.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/models/resource_list_model.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/workbench/workbench_context.h>

#include <nx/utils/string.h>


namespace
{
    class QnResourcesListSortModel : public QSortFilterProxyModel
    {
        typedef QSortFilterProxyModel base_type;

    public:
        explicit QnResourcesListSortModel(QObject *parent = 0) :
            base_type(parent),
            m_allChecked(false)
        {
        }

        virtual ~QnResourcesListSortModel()
        {
        }

        QnResourcePtr resource(const QModelIndex &index) const
        {
            return index.data(Qn::ResourceRole).value<QnResourcePtr>();
        }

        QVariant data(const QModelIndex &index, int role) const
        {
            if (m_allChecked && role == Qt::CheckStateRole && index.column() == QnResourceListModel::CheckColumn)
                return QVariant::fromValue<int>(Qt::Checked);

            return base_type::data(index, role);
        }

        bool allChecked() const
        {
            return m_allChecked;
        }

        void setAllChecked(bool value)
        {
            if (m_allChecked == value)
                return;

            m_allChecked = value;

            emit dataChanged(
                index(0, QnResourceListModel::CheckColumn),
                index(rowCount() - 1, QnResourceListModel::CheckColumn),
                { Qt::CheckStateRole });
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

            if (l->hasFlags(Qn::layout) && r->hasFlags(Qn::layout))
            {
                QnLayoutResourcePtr leftLayout = l.dynamicCast<QnLayoutResource>();
                QnLayoutResourcePtr rightLayout = r.dynamicCast<QnLayoutResource>();
                NX_ASSERT(leftLayout && rightLayout);
                if (!leftLayout || !rightLayout)
                    return leftLayout < rightLayout;

                if (leftLayout->isShared() != rightLayout->isShared())
                    return leftLayout->isShared();
            }

            {
                /* Sort by name. */
                QString leftDisplay = left.data(Qt::DisplayRole).toString();
                QString rightDisplay = right.data(Qt::DisplayRole).toString();
                int result = nx::utils::naturalStringCompare(leftDisplay, rightDisplay, Qt::CaseInsensitive);
                if (result != 0)
                    return result < 0;
            }

            /* We want the order to be defined even for items with the same name. */
            return l->getUniqueId() < r->getUniqueId();
        }

    private:
        bool m_allChecked;
    };

    const QString kDummyResourceId(lit("dummy_resource"));
}

QnAccessibleResourcesWidget::QnAccessibleResourcesWidget(QnAbstractPermissionsModel* permissionsModel, QnResourceAccessFilter::Filter filter, QWidget* parent /*= 0*/) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
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

    connect(this, &QnAccessibleResourcesWidget::controlsChanged, viewModel, &QnResourcesListSortModel::setAllChecked);

    ui->resourcesTreeView->setModel(viewModel);
    ui->controlsTreeView->setModel(m_controlsModel.data());
    ui->controlsTreeView->setVisible(m_controlsVisible);
    ui->controlsTreeView->setEnabled(m_controlsVisible);
    ui->line->setVisible(m_controlsVisible);

    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(ui->resourcesListWidget);
    scrollBar->setUseItemViewPaddingWhenVisible(false);
    scrollBar->setUseMaximumSpace(true);
    ui->resourcesTreeView->setVerticalScrollBar(scrollBar->proxyScrollBar());

    auto itemDelegate = new QnResourceItemDelegate(this);
    itemDelegate->setCustomInfoLevel(Qn::RI_FullInfo);

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

    auto batchToggleCheckboxes = [this](const QModelIndex& index)
    {
        QAbstractItemModel* model = static_cast<QnTreeView*>(sender())->model();
        QModelIndex checkboxIdx = index.sibling(index.row(), QnResourceListModel::CheckColumn);
        QModelIndexList selectedRows = ui->resourcesTreeView->selectionModel()->selectedRows(QnResourceListModel::CheckColumn);
        selectedRows << checkboxIdx;

        /* If any of selected rows were unchecked check all, otherwise uncheck all: */
        bool wasUnchecked = boost::algorithm::any_of(selectedRows, [this](const QModelIndex& index)
        {
            return index.data(Qt::CheckStateRole).toInt() != Qt::Checked;
        });

        int newCheckValue = wasUnchecked ? Qt::Checked : Qt::Unchecked;

        for (QModelIndex index : selectedRows)
            model->setData(index, newCheckValue, Qt::CheckStateRole);
    };

    connect(ui->resourcesTreeView, &QnTreeView::spacePressed, this, batchToggleCheckboxes);
    connect(ui->controlsTreeView,  &QnTreeView::spacePressed, this, batchToggleCheckboxes);

    connect(ui->resourcesTreeView, &QAbstractItemView::entered, this, &QnAccessibleResourcesWidget::updateThumbnail);
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

    if (m_filter == QnResourceAccessFilter::LayoutsFilter)
    {
        QnLayoutResourceList layoutsToShare = qnResPool->getResources(newFiltered)
            .filtered<QnLayoutResource>([](const QnLayoutResourcePtr& layout)
        {
            return !layout->isShared();
        });
        for (const auto& layout : layoutsToShare)
        {
            layout->setParentId(QnUuid());
            NX_ASSERT(layout->isShared());
            menu()->trigger(QnActions::SaveLayoutAction, QnActionParameters(layout));
        }
    }

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
    dummy->setName(tr("All Media Resources"));
    /* Create separate dummy resource id for each filter, but once per application run. */
    dummy->setId(QnUuid::createUuidFromPool(guidFromArbitraryData(kDummyResourceId).getQUuid(), m_filter));
    m_controlsModel->setResources(QnResourceList() << dummy);
    m_controlsModel->setCheckable(true);
    m_controlsModel->setStatusIgnored(true);

    auto modelUpdated = [this](const QModelIndex& index = QModelIndex())
    {
        QModelIndex checkedIdx = index.isValid() ?
            index.sibling(index.row(), QnResourceListModel::CheckColumn) :
            m_controlsModel->index(0, QnResourceListModel::CheckColumn);

        bool checked = checkedIdx.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        ui->resourcesTreeView->setEnabled(!checked);
        emit controlsChanged(checked);
        emit hasChangesChanged();
    };

    connect(m_controlsModel.data(), &QnResourceListModel::dataChanged, this, modelUpdated);
    connect(m_controlsModel.data(), &QnResourceListModel::modelReset,  this, modelUpdated);
}

void QnAccessibleResourcesWidget::initResourcesModel()
{
    m_resourcesModel->setCheckable(true);
    m_resourcesModel->setStatusIgnored(true);

    auto resourcePassFilter = [this](const QnResourcePtr& resource)
    {
        switch (m_filter)
        {
        case QnResourceAccessFilter::CamerasFilter:
            if (resource->hasFlags(Qn::desktop_camera))
                return false;
            return (resource->hasFlags(Qn::web_page) || resource->hasFlags(Qn::server_live_cam));

        case QnResourceAccessFilter::LayoutsFilter:
        {
            if (!context()->user())
                return false;

            if (!resource->hasFlags(Qn::layout))
                return false;

            QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
            if (!layout)
                return false;

                /* Hide "Preview Search" layouts */
            if (layout->data().contains(Qn::LayoutSearchStateRole)) //TODO: #GDM make it consistent with QnWorkbenchLayout::isSearchLayout
                return false;

            if (qnResPool->isAutoGeneratedLayout(layout))
                return false;

            if (layout->isFile())
                return false;

            return !layout->hasFlags(Qn::local) &&
                (layout->isShared() || layout->getParentId() == context()->user()->getId());
        }

        default:
            break;
        }
        return false;
    };

    auto handleResourceAdded = [this, resourcePassFilter](const QnResourcePtr& resource)
    {
        if (!resourcePassFilter(resource))
            return;
        m_resourcesModel->addResource(resource);
    };

    connect(qnResPool, &QnResourcePool::resourceAdded, this, handleResourceAdded);
    for (const QnResourcePtr& resource : qnResPool->getResources())
        handleResourceAdded(resource);

    connect(qnResPool, &QnResourcePool::resourceRemoved, this, [this](const QnResourcePtr& resource)
    {
        m_resourcesModel->removeResource(resource);
        auto accessibleResources = m_permissionsModel->accessibleResources();
        if (accessibleResources.contains(resource->getId()))
        {
            accessibleResources.remove(resource->getId());
            m_permissionsModel->setAccessibleResources(accessibleResources);
        }
    });


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

