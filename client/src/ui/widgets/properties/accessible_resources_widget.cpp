#include "accessible_resources_widget.h"
#include "ui_accessible_resources_widget.h"

#include <client/client_globals.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/camera_resource.h>

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

}

QnAccessibleResourcesWidget::QnAccessibleResourcesWidget(QnAbstractPermissionsModel* permissionsModel, QnAbstractPermissionsModel::Filter filter, QWidget* parent /*= 0*/):
    base_type(parent),
    ui(new Ui::AccessibleResourcesWidget()),
    m_permissionsModel(permissionsModel),
    m_filter(filter),
    m_resourcesModel(new QnResourceListModel()),
    m_viewModel(new QnResourcesListSortModel())
{
    ui->setupUi(this);

    switch (m_filter)
    {
    case QnAbstractPermissionsModel::CamerasFilter:
        ui->allResourcesCheckBox->setText(tr("All Cameras"));
        break;
    case QnAbstractPermissionsModel::LayoutsFilter:
        ui->allResourcesCheckBox->setText(tr("All Global Layouts"));
        ui->descriptionLabel->setText(tr("Giving access to some layouts you give access to all cameras on them. Also user will get access to all new cameras on these layouts."));
        break;
    case QnAbstractPermissionsModel::ServersFilter:
        ui->allResourcesCheckBox->setText(tr("All Servers"));
        ui->descriptionLabel->setText(tr("Giving access to some server you give access to view server statistics."));
        break;
    default:
        break;
    }
    ui->allResourcesCheckBox->setText(QnAbstractPermissionsModel::accessPermissionText(m_filter));

    m_resourcesModel->setCheckable(true);
    m_resourcesModel->setResources(QnAbstractPermissionsModel::filteredResources(m_filter, qnResPool->getResources()));
    connect(m_resourcesModel.data(), &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
    {
        if (roles.contains(Qt::CheckStateRole)
            && topLeft.column() <= QnResourceListModel::CheckColumn
            && bottomRight.column() >= QnResourceListModel::CheckColumn
            )
            emit hasChangesChanged();
    });

    m_viewModel->setSourceModel(m_resourcesModel.data());
    m_viewModel->setDynamicSortFilter(true);
    m_viewModel->setSortRole(Qt::DisplayRole);
    m_viewModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_viewModel->sort(Qn::NameColumn);


    auto itemDelegate = new QnResourceItemDelegate(this);
    ui->resourcesTreeView->setItemDelegate(itemDelegate);

    ui->resourcesTreeView->setModel(m_viewModel.data());
    ui->resourcesTreeView->setMouseTracking(true);

    ui->resourcesTreeView->header()->setStretchLastSection(false);
    ui->resourcesTreeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->resourcesTreeView->header()->setSectionResizeMode(QnResourceListModel::NameColumn, QHeaderView::Stretch);

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

    connect(ui->resourcesTreeView, &QnTreeView::spacePressed, this, [this](const QModelIndex& index)
    {
        QModelIndex checkedIdx = index.sibling(index.row(), Qn::CheckColumn);
        bool checked = checkedIdx.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        int inverted = checked ? Qt::Unchecked : Qt::Checked;
        m_viewModel->setData(checkedIdx, inverted, Qt::CheckStateRole);
    });

    updateThumbnail(QModelIndex());
}

QnAccessibleResourcesWidget::~QnAccessibleResourcesWidget()
{

}

bool QnAccessibleResourcesWidget::hasChanges() const
{
    if (m_permissionsModel->rawPermissions().testFlag(QnAbstractPermissionsModel::accessPermission(m_filter)) != ui->allResourcesCheckBox->isChecked())
        return true;

    return m_resourcesModel->checkedResources() != QnAbstractPermissionsModel::filteredResources(m_filter, m_permissionsModel->accessibleResources());
}

void QnAccessibleResourcesWidget::loadDataToUi()
{
    ui->allResourcesCheckBox->setChecked(m_permissionsModel->rawPermissions().testFlag(QnAbstractPermissionsModel::accessPermission(m_filter)));
    m_resourcesModel->setCheckedResources(QnAbstractPermissionsModel::filteredResources(m_filter, m_permissionsModel->accessibleResources()));
}

void QnAccessibleResourcesWidget::applyChanges()
{
    auto accessibleResources = m_permissionsModel->accessibleResources();
    auto oldFiltered = QnAbstractPermissionsModel::filteredResources(m_filter, accessibleResources);
    auto newFiltered = m_resourcesModel->checkedResources();
    accessibleResources.subtract(oldFiltered);
    accessibleResources.unite(newFiltered);
    m_permissionsModel->setAccessibleResources(accessibleResources);

    Qn::GlobalPermissions permissions = m_permissionsModel->rawPermissions();
    if (ui->allResourcesCheckBox->isChecked())
        permissions |= QnAbstractPermissionsModel::accessPermission(m_filter);
    else
        permissions &= ~QnAbstractPermissionsModel::accessPermission(m_filter);
    m_permissionsModel->setRawPermissions(permissions);
}

