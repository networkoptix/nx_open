#include "resource_selection_dialog.h"
#include "ui_resource_selection_dialog.h"

#include <QtCore/QIdentityProxyModel>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QPushButton>

#include <camera/camera_thumbnail_manager.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>

#include <ui/common/palette.h>
#include <ui/models/resource_pool_model.h>
#include <ui/style/globals.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/scoped_value_rollback.h>

namespace {
    class QnColoringProxyModel: public QIdentityProxyModel {
    public:
        QnColoringProxyModel(QnResourceSelectionDialogDelegate* delegate, QObject *parent = 0):
            QIdentityProxyModel(parent),
            m_delegate(delegate)
        {
        }

        QVariant data(const QModelIndex &proxyIndex, int role) const override {
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
    QnWorkbenchContextAware(parent)
{
    init(target);
}

QnResourceSelectionDialog::QnResourceSelectionDialog(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    init(CameraResourceTarget);
}

void QnResourceSelectionDialog::init(SelectionTarget target) {
    m_delegate = NULL;
    m_tooltipResourceId = 0;
    m_screenshotIndex = 0;
    m_target = target;
    m_updating = false;

    ui.reset(new Ui::ResourceSelectionDialog);
    ui->setupUi(this);

    bool flat;
    Qn::NodeType rootNodeType;


    switch (target) {
    case UserResourceTarget:
        flat = true;
        rootNodeType = Qn::UsersNode;
        setWindowTitle(tr("Select users..."));
        ui->detailsWidget->hide();
        resize(minimumSize());
        break;
    case CameraResourceTarget:
        flat = false;
        rootNodeType = Qn::ServersNode;
        setWindowTitle(tr("Select cameras..."));
        break;
    default:
        qWarning() << "undefined resource selection dialog behaviour";
        flat = false;
        rootNodeType = Qn::RootNode;
        setWindowTitle(tr("Select resources..."));
        ui->detailsWidget->hide();
        resize(minimumSize());
        break;
    }
    m_resourceModel = new QnResourcePoolModel(rootNodeType, flat, this);

    connect(m_resourceModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(at_resourceModel_dataChanged()));

    ui->resourcesWidget->setModel(m_resourceModel);
    ui->resourcesWidget->setFilterVisible(true);
    ui->resourcesWidget->setEditingEnabled(false);
    ui->resourcesWidget->setSimpleSelectionEnabled(true);
    ui->resourcesWidget->treeView()->setMouseTracking(true);

    ui->delegateFrame->setVisible(false);

    if (target == CameraResourceTarget) {
        connect(ui->resourcesWidget->treeView(), SIGNAL(entered(QModelIndex)), this, SLOT(updateThumbnail(QModelIndex)));
        m_thumbnailManager = new QnCameraThumbnailManager(this);
        m_thumbnailManager->setThumbnailSize(ui->screenshotLabel->contentSize());
        connect(m_thumbnailManager, SIGNAL(thumbnailReady(int,QPixmap)), this, SLOT(at_thumbnailReady(int, QPixmap)));
        updateThumbnail(QModelIndex());
    }

    at_resourceModel_dataChanged();
}

QnResourceSelectionDialog::~QnResourceSelectionDialog() {
    return;
}

QnResourceList QnResourceSelectionDialog::selectedResources() const {
    return selectedResourcesInner();
}

void QnResourceSelectionDialog::setSelectedResources(const QnResourceList &selected) {
    {
        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        setSelectedResourcesInner(selected);
    }

    at_resourceModel_dataChanged();
}

QnResourceList QnResourceSelectionDialog::selectedResourcesInner(const QModelIndex &parent) const {
    QnResourceList result;
    for (int i = 0; i < m_resourceModel->rowCount(parent); ++i){
        QModelIndex idx = m_resourceModel->index(i, Qn::NameColumn, parent);
        if (m_resourceModel->rowCount(idx) > 0)
            result.append(selectedResourcesInner(idx));

        QModelIndex checkedIdx = idx.sibling(i, Qn::CheckColumn);
        bool checked = checkedIdx.data(Qt::CheckStateRole) == Qt::Checked;
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

int QnResourceSelectionDialog::setSelectedResourcesInner(const QnResourceList &selected, const QModelIndex &parent) {
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

void QnResourceSelectionDialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        event->ignore();
        return;
    }
    base_type::keyPressEvent(event);
}

bool QnResourceSelectionDialog::event(QEvent *event) {
    bool result = base_type::event(event);

    if(event->type() == QEvent::Polish) {
    }

    return result;
}

void QnResourceSelectionDialog::setDelegate(QnResourceSelectionDialogDelegate *delegate) {
    Q_ASSERT(!m_delegate);
    m_delegate = delegate;
    if (m_delegate) {
        m_delegate->init(ui->delegateFrame);

        QnColoringProxyModel* proxy = new QnColoringProxyModel(m_delegate, this);
        proxy->setSourceModel(m_resourceModel);
        ui->resourcesWidget->setModel(proxy);
    }
    ui->delegateFrame->setVisible(m_delegate && ui->delegateLayout->count() > 0);
    at_resourceModel_dataChanged();
}

QnResourceSelectionDialogDelegate* QnResourceSelectionDialog::delegate() const {
    return m_delegate;
}

QModelIndex QnResourceSelectionDialog::itemIndexAt(const QPoint &pos) const {
    QAbstractItemView *treeView = ui->resourcesWidget->treeView();
    if(!treeView->model())
        return QModelIndex();
    QPoint childPos = treeView->mapFrom(const_cast<QnResourceSelectionDialog *>(this), pos);
    return treeView->indexAt(childPos);
}

void QnResourceSelectionDialog::updateThumbnail(const QModelIndex &index) {
    QVariant toolTip = index.data(Qt::ToolTipRole);
    QString toolTipText = toolTip.convert(QVariant::String) ? toolTip.toString() : QString();
    ui->detailsLabel->setText(toolTipText);

    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if (resource && (resource->flags() & QnResource::live_cam) && resource.dynamicCast<QnNetworkResource>()) {
        m_tooltipResourceId = resource->getId();
        m_thumbnailManager->selectResource(resource);
        ui->screenshotWidget->show();
    } else
        ui->screenshotWidget->hide();
}

void QnResourceSelectionDialog::at_resourceModel_dataChanged() {
    if (m_updating)
        return;
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!m_delegate || m_delegate->validate(selectedResources()));
}

void QnResourceSelectionDialog::at_thumbnailReady(int resourceId, const QPixmap &thumbnail) {
    if (m_tooltipResourceId != resourceId)
        return;
    m_screenshotIndex = 1 - m_screenshotIndex;
    if (m_screenshotIndex == 0)
        ui->screenshotLabel->setPixmap(thumbnail);
    else
        ui->screenshotLabel_2->setPixmap(thumbnail);
    ui->screenshotWidget->setCurrentIndex(m_screenshotIndex);
}
