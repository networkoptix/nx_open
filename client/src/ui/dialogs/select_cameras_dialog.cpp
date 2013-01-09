#include "select_cameras_dialog.h"
#include "ui_select_cameras_dialog.h"

#include <core/resource_managment/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <ui/models/resource_pool_model.h>

#include <ui/workbench/workbench_context.h>

QnSelectCamerasDialog::QnSelectCamerasDialog(QWidget *parent, QnWorkbenchContext *context) :
    base_type(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent),
    ui(new Ui::QnSelectCamerasDialog)
{
    ui->setupUi(this);

    m_resourceModel = new QnResourcePoolModel(this, Qn::ServersNode);
    connect(m_resourceModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(at_resourceModel_dataChanged()));
    ui->resourcesWidget->setModel(m_resourceModel);
    ui->resourcesWidget->setFilterVisible(true);

    at_resourceModel_dataChanged();
}

QnSelectCamerasDialog::~QnSelectCamerasDialog()
{
    delete ui;
}

QnVirtualCameraResourceList QnSelectCamerasDialog::getSelectedCameras() const {
    QnResourceList result;
    for (int i = 0; i < m_resourceModel->rowCount(); ++i){
        //servers
        QModelIndex idx = m_resourceModel->index(i, Qn::NameColumn);
        for (int j = 0; j < m_resourceModel->rowCount(idx); ++j){
            //cameras
            QModelIndex camIdx = m_resourceModel->index(j, Qn::NameColumn, idx);
            QModelIndex checkedIdx = camIdx.sibling(camIdx.row(), Qn::CheckColumn);
            bool checked = checkedIdx.data(Qt::CheckStateRole) == Qt::Checked;
            if (!checked)
                continue;
            QnResourcePtr resource = camIdx.data(Qn::ResourceRole).value<QnResourcePtr>();
            if(resource)
                 result.append(resource);
        }
    }
    QnVirtualCameraResourceList cameras = result.filtered<QnVirtualCameraResource>(); //safety check
    return cameras;
}

void QnSelectCamerasDialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        event->ignore();
        return;
    }
    base_type::keyPressEvent(event);
}

// ------------ Handlers ---------------------

void QnSelectCamerasDialog::at_resourceModel_dataChanged(){
//
}
