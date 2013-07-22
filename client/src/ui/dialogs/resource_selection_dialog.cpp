
#include "resource_selection_dialog.h"

#include <QtGui/QKeyEvent>
#include <QtWidgets/QPushButton>

#include "ui_resource_selection_dialog.h"

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

    ui.reset(new Ui::QnResourceSelectionDialog);
    ui->setupUi(this);

    m_flat = rootNodeType == Qn::UsersNode; //TODO: #GDM servers?
    m_resourceModel = new QnResourcePoolModel(rootNodeType, m_flat, this);

    switch (rootNodeType) {
    case Qn::UsersNode:
        setWindowTitle(tr("Select users..."));
        break;
    case Qn::ServersNode:
        setWindowTitle(tr("Select cameras..."));
        break;
    default:
        setWindowTitle(tr("Select resources..."));
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

    ui->delegateFrame->setVisible(false);
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

void QnResourceSelectionDialog::at_resourceModel_dataChanged() {
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!m_delegate || m_delegate->validate(selectedResources()));
}

