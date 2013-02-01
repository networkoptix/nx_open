#include "select_cameras_dialog.h"
#include "ui_select_cameras_dialog.h"

#include <core/resource_managment/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <ui/models/resource_pool_model.h>

#include <ui/workbench/workbench_context.h>




///////////////////////////////////////////////////////////////////////////////////////
//---------------- QnSelectCamerasDialogDelegate ------------------------------------//
///////////////////////////////////////////////////////////////////////////////////////

QnSelectCamerasDialogDelegate::QnSelectCamerasDialogDelegate(QObject *parent):
    QObject(parent)
{

}

QnSelectCamerasDialogDelegate::~QnSelectCamerasDialogDelegate() {

}

void QnSelectCamerasDialogDelegate::setWidgetLayout(QLayout *layout) {
    Q_UNUSED(layout)
}

void QnSelectCamerasDialogDelegate::modelDataChanged(const QnResourceList &selected) {
    Q_UNUSED(selected)
}

bool QnSelectCamerasDialogDelegate::isApplyAllowed() {
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////
//---------------- QnSelectCamerasDialog --------------------------------------------//
///////////////////////////////////////////////////////////////////////////////////////

QnSelectCamerasDialog::QnSelectCamerasDialog(QWidget *parent, QnWorkbenchContext *context) :
    base_type(parent),
    QnWorkbenchContextAware(parent, context),
    ui(new Ui::QnSelectCamerasDialog),
    m_delegate(NULL)
{
    ui->setupUi(this);

    m_resourceModel = new QnResourcePoolModel(this, Qn::ServersNode);
    connect(m_resourceModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(at_resourceModel_dataChanged()));

    /*
    QnColoringProxyModel* proxy = new QnColoringProxyModel(this);
    proxy->setSourceModel(m_resourceModel);
    ui->resourcesWidget->setModel(proxy);
    */

    ui->resourcesWidget->setModel(m_resourceModel);
    ui->resourcesWidget->setFilterVisible(true);

    ui->delegateFrame->setVisible(false);
}

QnSelectCamerasDialog::~QnSelectCamerasDialog()
{
}

QnResourceList QnSelectCamerasDialog::getSelectedResources() const {
    QnResourceList result;
    for (int i = 0; i < m_resourceModel->rowCount(); ++i){
        //servers
        QModelIndex idx = m_resourceModel->index(i, Qn::NameColumn);
        for (int j = 0; j < m_resourceModel->rowCount(idx); ++j){
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
    return result;
}

void QnSelectCamerasDialog::setSelectedResources(const QnResourceList &selected) {
    for (int i = 0; i < m_resourceModel->rowCount(); ++i){
        //servers
        QModelIndex idx = m_resourceModel->index(i, Qn::NameColumn);
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

void QnSelectCamerasDialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        event->ignore();
        return;
    }
    base_type::keyPressEvent(event);
}

void QnSelectCamerasDialog::setDelegate(QnSelectCamerasDialogDelegate *delegate) {
    Q_ASSERT(!m_delegate);
    m_delegate = delegate;
    if (m_delegate) {
        m_delegate->setWidgetLayout(ui->delegateLayout);
    }
    ui->delegateFrame->setVisible(m_delegate && ui->delegateLayout->count() > 0);
    at_resourceModel_dataChanged();
}

QnSelectCamerasDialogDelegate* QnSelectCamerasDialog::delegate() {
    return m_delegate;
}

void QnSelectCamerasDialog::at_resourceModel_dataChanged() {
    if (m_delegate)
        m_delegate->modelDataChanged(getSelectedResources());
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!m_delegate || m_delegate->isApplyAllowed());
}

