#include "resource_selection_dialog.h"
#include "ui_resource_selection_dialog.h"

#include <QtCore/QIdentityProxyModel>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QPushButton>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>

#include <ui/common/palette.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/models/resource/resource_tree_model.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/workbench/workbench_context.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <utils/common/event_processors.h>
#include <utils/common/scoped_value_rollback.h>

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
            case Qt::TextColorRole:
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

} // namespace

QnResourceSelectionDialog::QnResourceSelectionDialog(Filter filter, QWidget* parent):
    base_type(parent),
    ui(new Ui::ResourceSelectionDialog),
    m_resourceModel(NULL),
    m_delegate(NULL),
    m_filter(filter),
    m_updating(false)
{
    ui->setupUi(this);
    setHelpTopic(ui->resourcesWidget->treeView(), Qn::Forced_Empty_Help);

    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(ui->treeWidget);
    scrollBar->setUseItemViewPaddingWhenVisible(false);
    scrollBar->setUseMaximumSpace(true);
    ui->resourcesWidget->treeView()->setVerticalScrollBar(scrollBar->proxyScrollBar());

    installEventHandler(scrollBar, { QEvent::Show, QEvent::Hide }, this,
        [this](QObject* watched, QEvent* event)
        {
            Q_UNUSED(watched);
            int margin = style()->pixelMetric(QStyle::PM_DefaultTopLevelMargin);
            if (event->type() == QEvent::Hide)
                margin -= style()->pixelMetric(QStyle::PM_ScrollBarExtent);

            ui->treeWidget->setContentsMargins(0, 0, margin, 0);
        });

    switch (m_filter)
    {
        case Filter::users:
            setWindowTitle(tr("Select users..."));
            ui->detailsWidget->hide();
            resize(minimumSize());
            break;

        case Filter::cameras:
            setWindowTitle(QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("Select Devices..."),
                tr("Select Cameras...")));
            break;

        default:
            NX_ASSERT(false, "Should never get here");
            ui->detailsWidget->hide();
            resize(minimumSize());
            break;
    }

    initModel();
}

void QnResourceSelectionDialog::initModel()
{
    QnResourceTreeModel::Scope scope;

    switch (m_filter)
    {
        case Filter::users:
            scope = QnResourceTreeModel::UsersScope;
            break;
        case Filter::cameras:
            scope = QnResourceTreeModel::CamerasScope;
            break;
        default:
            NX_ASSERT(false, "Should never get here");
            scope = QnResourceTreeModel::FullScope;
            break;
    }

    m_resourceModel = new QnResourceTreeModel(scope, this);

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

    if (m_filter == Filter::cameras)
    {
        connect(ui->resourcesWidget->treeView(), &QAbstractItemView::entered, this,
            &QnResourceSelectionDialog::updateThumbnail);
        updateThumbnail(QModelIndex());
    }

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
        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
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

        auto nodeType = idx.data(Qn::NodeTypeRole).value<Qn::NodeType>();
        auto resource = idx.data(Qn::ResourceRole).value<QnResourcePtr>();
        auto id = idx.data(Qn::UuidRole).value<QnUuid>();

        switch (m_filter)
        {
            case QnResourceSelectionDialog::Filter::cameras:
                if (resource.dynamicCast<QnVirtualCameraResource>())
                    result.insert(resource->getId());
                break;
            case QnResourceSelectionDialog::Filter::users:
                if (resource.dynamicCast<QnUserResource>())
                    result.insert(resource->getId());
                if (nodeType == Qn::RoleNode)
                    result.insert(id);
                break;
            default:
                break;
        }
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
