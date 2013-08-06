
#include "resource_selection_dialog.h"

#include <QKeyEvent>
#include <QPushButton>

#include "ui_resource_selection_dialog.h"

#include <camera/camera_thumbnail_manager.h>

#include <core/resource_managment/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

#include <ui/models/resource_pool_model.h>

#include <ui/workbench/workbench_context.h>

// -------------------------------------------------------------------------- //
// QnResourceSelectionDialog
// -------------------------------------------------------------------------- //
QnResourceSelectionDialog::QnResourceSelectionDialog(Qn::NodeType rootNodeType, QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    init(rootNodeType);
}

QnResourceSelectionDialog::QnResourceSelectionDialog(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    init(Qn::ServersNode);
}

void QnResourceSelectionDialog::init(Qn::NodeType rootNodeType) {
    m_delegate = NULL;
    m_tooltipResourceId = 0;

    ui.reset(new Ui::QnResourceSelectionDialog);
    ui->setupUi(this);

    m_flat = rootNodeType == Qn::UsersNode; //TODO: #GDM servers?
    m_resourceModel = new QnResourcePoolModel(rootNodeType, m_flat, this);

    switch (rootNodeType) {
    case Qn::UsersNode:
        setWindowTitle(tr("Select users..."));
        ui->detailsWidget->hide();
        resize(minimumSize());
        break;
    case Qn::ServersNode:
        setWindowTitle(tr("Select cameras..."));
        break;
    default:
        setWindowTitle(tr("Select resources..."));
        ui->detailsWidget->hide();
        resize(minimumSize());
        break;
    }

    connect(m_resourceModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(at_resourceModel_dataChanged()));

    /*
    QnColoringProxyModel* proxy = new QnColoringProxyModel(this);
    proxy->setSourceModel(m_resourceModel);
    ui->resourcesWidget->setModel(proxy);
    */

    ui->resourcesWidget->setModel(m_resourceModel);
    ui->resourcesWidget->setFilterVisible(true);
    ui->resourcesWidget->setEditingEnabled(false);
    ui->resourcesWidget->setSimpleSelectionEnabled(true);
    ui->resourcesWidget->treeView()->setMouseTracking(true);

    connect(ui->resourcesWidget->treeView(), SIGNAL(entered(QModelIndex)), this, SLOT(updateThumbnail(QModelIndex)));
    ui->delegateFrame->setVisible(false);

    m_thumbnailManager = new QnCameraThumbnailManager(this);
    connect(m_thumbnailManager, SIGNAL(thumbnailReady(int,QPixmap)), this, SLOT(at_thumbnailReady(int, QPixmap)));
    updateThumbnail(QModelIndex());
}

QnResourceSelectionDialog::~QnResourceSelectionDialog() {
    return;
}

QnResourceList QnResourceSelectionDialog::selectedResources() const {
    QnResourceList result;
    for (int i = 0; i < m_resourceModel->rowCount(); ++i){
        //root nodes
        QModelIndex idx = m_resourceModel->index(i, Qn::NameColumn);
        if (m_flat) {
            QModelIndex checkedIdx = idx.sibling(i, Qn::CheckColumn);
            bool checked = checkedIdx.data(Qt::CheckStateRole) == Qt::Checked;
            if (!checked)
                continue;
            QnResourcePtr resource = idx.data(Qn::ResourceRole).value<QnResourcePtr>();
            if(resource)
                 result.append(resource);
        } else {
            for (int j = 0; j < m_resourceModel->rowCount(idx); ++j) {
                //cameras
                QModelIndex camIdx = m_resourceModel->index(j, Qn::NameColumn, idx);
                QModelIndex checkedIdx = camIdx.sibling(j, Qn::CheckColumn);
                bool checked = checkedIdx.data(Qt::CheckStateRole) == Qt::Checked;
                if (!checked)
                    continue;
                QnResourcePtr resource = camIdx.data(Qn::ResourceRole).value<QnResourcePtr>();
                if(resource)
                     result.append(resource);
            }
        }
    }
    return result;
}

void QnResourceSelectionDialog::setSelectedResources(const QnResourceList &selected) {
    for (int i = 0; i < m_resourceModel->rowCount(); ++i){
        //root nodes
        QModelIndex idx = m_resourceModel->index(i, Qn::NameColumn);
        if (m_flat) {
            QModelIndex checkedIdx = idx.sibling(i, Qn::CheckColumn);
            QnResourcePtr resource = idx.data(Qn::ResourceRole).value<QnResourcePtr>();
            bool checked = selected.contains(resource);
            m_resourceModel->setData(checkedIdx,
                                     checked ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
        } else {
            for (int j = 0; j < m_resourceModel->rowCount(idx); ++j){
                //cameras
                QModelIndex camIdx = m_resourceModel->index(j, Qn::NameColumn, idx);
                QModelIndex checkedIdx = camIdx.sibling(j, Qn::CheckColumn);

                QnResourcePtr resource = camIdx.data(Qn::ResourceRole).value<QnResourcePtr>();
                bool checked = selected.contains(resource);
                m_resourceModel->setData(checkedIdx,
                                         checked ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
            }
        }
    }
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
        ui->screenshotLabel->show();
    } else
        ui->screenshotLabel->hide();
}

void QnResourceSelectionDialog::at_resourceModel_dataChanged() {
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!m_delegate || m_delegate->validate(selectedResources()));
}

void QnResourceSelectionDialog::at_thumbnailReady(int resourceId, const QPixmap &thumbnail) {
    if (m_tooltipResourceId != resourceId)
        return;
    ui->screenshotLabel->setPixmap(thumbnail);
}
