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
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/workbench/workbench_context.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <utils/common/event_processors.h>
#include <utils/common/scoped_value_rollback.h>

namespace
{
    class QnColoringProxyModel: public QIdentityProxyModel
    {
    public:
        QnColoringProxyModel(QnResourceSelectionDialogDelegate* delegate, QObject *parent = 0):
            QIdentityProxyModel(parent),
            m_delegate(delegate)
        {}

        QVariant data(const QModelIndex &proxyIndex, int role) const override
        {
            if (role == Qt::TextColorRole && m_delegate && !m_delegate->isValid(resource(proxyIndex)))
                return QBrush(QColor(qnGlobals->errorTextColor()));
            return QIdentityProxyModel::data(proxyIndex, role);
        }

    private:
        QnResourcePtr resource(const QModelIndex &proxyIndex) const {
            return QIdentityProxyModel::data(proxyIndex, Qn::ResourceRole).value<QnResourcePtr>();
        }

        QnResourceSelectionDialogDelegate* m_delegate;
    };
}

// -------------------------------------------------------------------------- //
// QnResourceSelectionDialog
// -------------------------------------------------------------------------- //
QnResourceSelectionDialog::QnResourceSelectionDialog(SelectionTarget target, QWidget *parent):
    base_type(parent),
    ui(new Ui::ResourceSelectionDialog),
    m_resourceModel(NULL),
    m_delegate(NULL),
    m_target(target),
    m_updating(false)
{
    ui->setupUi(this);

    setHelpTopic(ui->resourcesWidget->treeView(), Qn::Forced_Empty_Help);

    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(ui->treeWidget);
    scrollBar->setUseItemViewPaddingWhenVisible(false);
    scrollBar->setUseMaximumSpace(true);
    ui->resourcesWidget->treeView()->setVerticalScrollBar(scrollBar->proxyScrollBar());

    auto showHideSignalizer = new QnMultiEventSignalizer(this);
    showHideSignalizer->addEventType(QEvent::Show);
    showHideSignalizer->addEventType(QEvent::Hide);
    scrollBar->installEventFilter(showHideSignalizer);
    connect(showHideSignalizer, &QnMultiEventSignalizer::activated, this,
        [this](QObject* watched, QEvent* event)
        {
            Q_UNUSED(watched);
            int margin = style()->pixelMetric(QStyle::PM_DefaultTopLevelMargin);
            if (event->type() == QEvent::Hide)
                margin -= style()->pixelMetric(QStyle::PM_ScrollBarExtent);

            ui->treeWidget->setContentsMargins(0, 0, margin, 0);
        });

    switch (m_target)
    {
        case UserResourceTarget:
            setWindowTitle(tr("Select Users..."));
            ui->detailsWidget->hide();
            resize(minimumSize());
            break;

        case CameraResourceTarget:
            setWindowTitle(QnDeviceDependentStrings::getDefaultNameFromSet(
                tr("Select Devices..."),
                tr("Select Cameras...")));
            break;

        default:
            setWindowTitle(tr("Select Resources..."));
            ui->detailsWidget->hide();
            resize(minimumSize());
            break;
    }

    initModel();
}


QnResourceSelectionDialog::QnResourceSelectionDialog(QWidget *parent) :
    QnResourceSelectionDialog(CameraResourceTarget, parent)
{}

void QnResourceSelectionDialog::initModel()
{
    QnResourceTreeModel::Scope scope;

    switch (m_target) {
    case UserResourceTarget:
        scope = QnResourceTreeModel::UsersScope;
        break;
    case CameraResourceTarget:
        scope = QnResourceTreeModel::CamerasScope;
        break;
    default:
        scope = QnResourceTreeModel::FullScope;
        break;
    }

    m_resourceModel = new QnResourceTreeModel(scope, this);

    connect(m_resourceModel, &QnResourceTreeModel::dataChanged, this, &QnResourceSelectionDialog::at_resourceModel_dataChanged);

    ui->resourcesWidget->setModel(m_resourceModel);
    ui->resourcesWidget->setFilterVisible(true);
    ui->resourcesWidget->setEditingEnabled(false);
    ui->resourcesWidget->setSimpleSelectionEnabled(true);
    ui->resourcesWidget->treeView()->setMouseTracking(true);
    ui->resourcesWidget->itemDelegate()->setCustomInfoLevel(Qn::RI_FullInfo);

    connect(ui->resourcesWidget, &QnResourceTreeWidget::beforeRecursiveOperation,   this, [this]{ m_updating = true;});
    connect(ui->resourcesWidget, &QnResourceTreeWidget::afterRecursiveOperation,   this, [this]{
        m_updating = false;
        at_resourceModel_dataChanged();
    });

    ui->delegateFrame->setVisible(false);

    if (m_target == CameraResourceTarget)
    {
        connect(ui->resourcesWidget->treeView(), &QAbstractItemView::entered, this, &QnResourceSelectionDialog::updateThumbnail);
        updateThumbnail(QModelIndex());
    }

    at_resourceModel_dataChanged();
}

QnResourceSelectionDialog::~QnResourceSelectionDialog() {}


QnResourceList QnResourceSelectionDialog::selectedResources() const
{
    return selectedResourcesInner();
}

void QnResourceSelectionDialog::setSelectedResources(const QnResourceList &selected)
{
    {
        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        setSelectedResourcesInner(selected);
    }
    at_resourceModel_dataChanged();
}

QnResourceList QnResourceSelectionDialog::selectedResourcesInner(const QModelIndex &parent) const
{
    QnResourceList result;
    for (int i = 0; i < m_resourceModel->rowCount(parent); ++i){
        QModelIndex idx = m_resourceModel->index(i, Qn::NameColumn, parent);
        if (m_resourceModel->rowCount(idx) > 0)
            result.append(selectedResourcesInner(idx));

        QModelIndex checkedIdx = idx.sibling(i, Qn::CheckColumn);
        bool checked = checkedIdx.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        if (!checked)
            continue;

        QnResourcePtr resource = idx.data(Qn::ResourceRole).value<QnResourcePtr>();
        if (m_target == UserResourceTarget && resource.dynamicCast<QnUserResource>())
            result.append(resource);

        if (m_target == CameraResourceTarget && resource.dynamicCast<QnVirtualCameraResource>())
            result.append(resource);
    }
    return result;
}

int QnResourceSelectionDialog::setSelectedResourcesInner(const QnResourceList &selected, const QModelIndex &parent)
{
    int count = 0;
    for (int i = 0; i < m_resourceModel->rowCount(parent); ++i) {
        QModelIndex idx = m_resourceModel->index(i, Qn::NameColumn, parent);
        QModelIndex checkedIdx = idx.sibling(i, Qn::CheckColumn);
        bool checked = false;

        int childCount = m_resourceModel->rowCount(idx);
        if (childCount > 0) {
            checked = (setSelectedResourcesInner(selected, idx) == childCount);
        } else {
            QnResourcePtr resource = idx.data(Qn::ResourceRole).value<QnResourcePtr>();
            if ((m_target == UserResourceTarget && resource.dynamicCast<QnUserResource>())
                    || (m_target == CameraResourceTarget && resource.dynamicCast<QnVirtualCameraResource>()))
                checked = selected.contains(resource);
        }

        if (checked)
            count++;
        m_resourceModel->setData(checkedIdx,
                                 checked ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
    }
    return count;
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

void QnResourceSelectionDialog::setDelegate(QnResourceSelectionDialogDelegate *delegate)
{
    NX_ASSERT(!m_delegate);
    m_delegate = delegate;
    if (m_delegate) {
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

QModelIndex QnResourceSelectionDialog::itemIndexAt(const QPoint &pos) const
{
    QAbstractItemView *treeView = ui->resourcesWidget->treeView();
    if(!treeView->model())
        return QModelIndex();
    QPoint childPos = treeView->mapFrom(const_cast<QnResourceSelectionDialog *>(this), pos);
    return treeView->indexAt(childPos);
}

void QnResourceSelectionDialog::updateThumbnail(const QModelIndex &index)
{
    QModelIndex baseIndex = index.sibling(index.row(), Qn::NameColumn);
    QString toolTip = baseIndex.data(Qt::ToolTipRole).toString();
    ui->detailsWidget->setName(toolTip);
    ui->detailsWidget->setTargetResource(index.data(Qn::ResourceRole).value<QnResourcePtr>());
    ui->detailsWidget->layout()->activate();
}

void QnResourceSelectionDialog::at_resourceModel_dataChanged()
{
    if (m_updating)
        return;
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!m_delegate || m_delegate->validate(selectedResources()));
}
