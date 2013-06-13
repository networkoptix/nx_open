#include "camera_list_dialog.h"

#include <QClipboard>
#include <QMenu>

#include "ui_camera_list_dialog.h"
#include "ui/models/camera_list_model.h"
#include "ui/workbench/workbench_context.h"
#include "core/resource_managment/resource_pool.h"

QnCameraListDialog::QnCameraListDialog(QWidget *parent, QnWorkbenchContext *context):
    QDialog(parent),
    ui(new Ui::CameraListDialog)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    m_model = new QnCameraListModel(this);
    m_model->setResources(context->resourcePool()->getAllEnabledCameras());

    QList<QnCameraListModel::Column> columns;
    columns << QnCameraListModel::NameColumn << QnCameraListModel::VendorColumn << QnCameraListModel::ModelColumn <<
               QnCameraListModel::FirmwareColumn << QnCameraListModel::IPColumn << QnCameraListModel::UniqIdColumn;

    m_model->setColumns(columns);
    ui->gridCameras->setModel(m_model);
}

QnCameraListDialog::~QnCameraListDialog()
{
}
