#include "resource_selection_dialog.h"
#include "ui_resource_selection_dialog.h"

#include <QtCore/QIdentityProxyModel>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeView>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>

#include <nx/vms/client/desktop/resource_views/data/node_type.h>

#include <ui/common/palette.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource/resource_tree_model_node.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/widgets/resource_tree_widget.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <ui/workbench/workbench_context.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <utils/common/event_processors.h>

using namespace nx::vms::client::desktop;

namespace {

class QnColoringProxyModel: public QIdentityProxyModel
{
    using base_type = QIdentityProxyModel;
public:
    QnColoringProxyModel(QnResourceSelectionDialogDelegate* delegate, QObject* parent = nullptr):
        base_type(parent),
        m_delegate(delegate)
    {
    }

    QVariant data(const QModelIndex &proxyIndex, int role) const override
    {
        if (!m_delegate || m_delegate->isValid(id(proxyIndex)))
            return base_type::data(proxyIndex, role);

        // TODO: #GDM #3.1 refactor to common delegate
        // Handling only invalid rows here
        switch (role)
        {
            case Qt::DecorationRole:
            {
                if (proxyIndex.column() == Qn::NameColumn)
                {
                    auto resource = base_type::data(proxyIndex, Qn::ResourceRole).value<QnResourcePtr>();
                    if (resource && resource->hasFlags(Qn::user))
                        return qnSkin->icon("tree/user_error.png");
                }
                break;
            }
            case Qt::ForegroundRole:
            {
                return QBrush(qnGlobals->errorTextColor());
            }
        }

        return base_type::data(proxyIndex, role);
    }

private:
    QnUuid id(const QModelIndex &proxyIndex) const
    {
        auto resource = base_type::data(proxyIndex, Qn::ResourceRole).value<QnResourcePtr>();
        if (resource)
            return resource->getId();
        return base_type::data(proxyIndex, Qn::UuidRole).value<QnUuid>();
    }

    QnResourceSelectionDialogDelegate* m_delegate;
};

// Threshold for expanding camera list.
const int kAutoExpandThreshold = 50;

} // namespace

QnResourceSelectionDialog::QnResourceSelectionDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::ResourceSelectionDialog),
    m_resourceModel(NULL),
    m_delegate(NULL),
    m_updating(false)
{
    ui->setupUi(this);
    setHelpTopic(ui->resourcesWidget->treeView(), Qn::Forced_Empty_Help);

    SnappedScrollBar* scrollBar = new SnappedScrollBar(ui->treeWidget);
    scrollBar->setUseItemViewPaddingWhenVisible(false);
    scrollBar->setUseMaximumSpace(true);
    ui->resourcesWidget->treeView()->setVerticalScrollBar(scrollBar->proxyScrollBar());
    ui->resourcesWidget->treeView()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->resourcesWidget->treeView()->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);

    installEventHandler(scrollBar, { QEvent::Show, QEvent::Hide }, this,
        [this](QObject* watched, QEvent* event)
        {
            Q_UNUSED(watched);
            int margin = style()->pixelMetric(QStyle::PM_DefaultTopLevelMargin);
            if (event->type() == QEvent::Hide)
                margin -= style()->pixelMetric(QStyle::PM_ScrollBarExtent);

            ui->treeWidget->setContentsMargins(0, 0, margin, 0);
        });

    setWindowTitle(QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(),
        tr("Select Devices..."),
        tr("Select Cameras...")));

    initModel();
}

void QnResourceSelectionDialog::initModel()
{
    m_resourceModel = new QnResourceTreeModel(QnResourceTreeModel::CamerasScope, this);

    // Auto expand if and only if server count <= 1 or cameras count <= 50.
    if (auto treeRoot = m_resourceModel->rootNode(ResourceTreeNodeType::servers))
    {
        int numServers = treeRoot->children().size();
        int numResources = treeRoot->childrenRecursive().size() - numServers;
        bool expandAll = numServers <= 1 || numResources <= kAutoExpandThreshold;

        auto expandPolicy = [expandAll](const QModelIndex& index) { return expandAll; };
        ui->resourcesWidget->setAutoExpandPolicy(expandPolicy);
    }

    connect(m_resourceModel, &QnResourceTreeModel::dataChanged, this,
        &QnResourceSelectionDialog::at_resourceModel_dataChanged);

    ui->resourcesWidget->setModel(m_resourceModel);
    ui->resourcesWidget->setFilterVisible(true);
    ui->resourcesWidget->setEditingEnabled(false);
    ui->resourcesWidget->setSimpleSelectionEnabled(true);
    ui->resourcesWidget->treeView()->setMouseTracking(true);
    ui->resourcesWidget->itemDelegate()->setCustomInfoLevel(Qn::RI_FullInfo);

    connect(ui->resourcesWidget, &QnResourceTreeWidget::beforeRecursiveOperation, this,
        [this]
        {
            m_updating = true;
        });

    connect(ui->resourcesWidget, &QnResourceTreeWidget::afterRecursiveOperation, this,
        [this]
        {
            m_updating = false;
            at_resourceModel_dataChanged();
        });

    ui->delegateFrame->setVisible(false);

    connect(ui->resourcesWidget->treeView(), &QAbstractItemView::entered, this,
        &QnResourceSelectionDialog::updateThumbnail);
    updateThumbnail(QModelIndex());

    at_resourceModel_dataChanged();
}

QnResourceSelectionDialog::~QnResourceSelectionDialog() {}


QSet<QnUuid> QnResourceSelectionDialog::selectedResources() const
{
    return selectedResourcesInternal();
}

void QnResourceSelectionDialog::setSelectedResources(const QSet<QnUuid>& selected)
{
    {
        QScopedValueRollback<bool> guard(m_updating, true);
        setSelectedResourcesInternal(selected);
    }
    at_resourceModel_dataChanged();
    ui->resourcesWidget->expandChecked();
}

QSet<QnUuid> QnResourceSelectionDialog::selectedResourcesInternal(const QModelIndex& parent) const
{
    QSet<QnUuid> result;
    for (int i = 0; i < m_resourceModel->rowCount(parent); ++i)
    {
        QModelIndex idx = m_resourceModel->index(i, Qn::NameColumn, parent);
        if (m_resourceModel->rowCount(idx) > 0)
            result += selectedResourcesInternal(idx);

        QModelIndex checkedIdx = idx.sibling(i, Qn::CheckColumn);
        bool checked = checkedIdx.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        if (!checked)
            continue;

        auto nodeType = idx.data(Qn::NodeTypeRole).value<ResourceTreeNodeType>();
        auto resource = idx.data(Qn::ResourceRole).value<QnResourcePtr>();
        auto id = idx.data(Qn::UuidRole).value<QnUuid>();

        if (resource.dynamicCast<QnVirtualCameraResource>())
            result.insert(resource->getId());
    }
    return result;
}

void QnResourceSelectionDialog::setSelectedResourcesInternal(
    const QSet<QnUuid>& selected,
    const QModelIndex& parent)
{
    for (int i = 0; i < m_resourceModel->rowCount(parent); ++i)
    {
        QModelIndex idx = m_resourceModel->index(i, Qn::NameColumn, parent);

        int childCount = m_resourceModel->rowCount(idx);
        if (childCount > 0)
        {
            setSelectedResourcesInternal(selected, idx);
        }
        else
        {
            auto resource = idx.data(Qn::ResourceRole).value<QnResourcePtr>();
            bool checked = selected.contains(resource
                ? resource->getId()
                : idx.data(Qn::UuidRole).value<QnUuid>());

            m_resourceModel->setData(idx.sibling(i, Qn::CheckColumn),
                checked ? Qt::Checked : Qt::Unchecked,
                Qt::CheckStateRole);
        }
    }
}

void QnResourceSelectionDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
    {
        event->ignore();
        return;
    }
    base_type::keyPressEvent(event);
}

void QnResourceSelectionDialog::setDelegate(QnResourceSelectionDialogDelegate* delegate)
{
    NX_ASSERT(!m_delegate);
    m_delegate = delegate;
    if (m_delegate)
    {
        m_delegate->init(ui->delegateFrame);

        QnColoringProxyModel* proxy = new QnColoringProxyModel(m_delegate, this);
        proxy->setSourceModel(m_resourceModel);
        ui->resourcesWidget->setModel(proxy);
        ui->resourcesWidget->setCustomColumnDelegate(m_delegate->customColumnDelegate());

        setHelpTopic(ui->resourcesWidget->treeView(), m_delegate->helpTopicId());
    }

    ui->delegateFrame->setVisible(m_delegate && ui->delegateLayout->count() > 0);
    if (m_delegate && m_delegate->isFlat())
        ui->resourcesLayout->setSpacing(0);

    at_resourceModel_dataChanged();
}

QnResourceSelectionDialogDelegate* QnResourceSelectionDialog::delegate() const
{
    return m_delegate;
}

QModelIndex QnResourceSelectionDialog::itemIndexAt(const QPoint& pos) const
{
    QAbstractItemView *treeView = ui->resourcesWidget->treeView();
    if (!treeView->model())
        return QModelIndex();
    QPoint childPos = treeView->mapFrom(const_cast<QnResourceSelectionDialog *>(this), pos);
    return treeView->indexAt(childPos);
}

void QnResourceSelectionDialog::updateThumbnail(const QModelIndex& index)
{
    QModelIndex baseIndex = index.sibling(index.row(), Qn::NameColumn);
    QString toolTip = baseIndex.data(Qt::ToolTipRole).toString();
    ui->detailsWidget->setName(toolTip);
    ui->detailsWidget->setResource(index.data(Qn::ResourceRole).value<QnResourcePtr>());
    ui->detailsWidget->layout()->activate();
}

void QnResourceSelectionDialog::at_resourceModel_dataChanged()
{
    if (m_updating)
        return;

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
        !m_delegate || m_delegate->validate(selectedResources()));
}
