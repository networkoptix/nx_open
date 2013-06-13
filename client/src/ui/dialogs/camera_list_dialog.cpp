#include "camera_list_dialog.h"

#include <QClipboard>
#include <QMenu>

#include "ui_camera_list_dialog.h"
#include "ui/models/camera_list_model.h"
#include "ui/workbench/workbench_context.h"
#include "core/resource_managment/resource_pool.h"
#include "ui/models/resource_search_proxy_model.h"

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

    m_resourceSearch = new QnResourceSearchProxyModel(this);
    m_resourceSearch->setSourceModel(m_model);
    m_resourceSearch->addCriterion(QnResourceCriterion(QRegExp(lit("*"),Qt::CaseInsensitive, QRegExp::Wildcard)));
    
    connect(ui->SearchString, SIGNAL(textChanged(const QString&)), this, SLOT(at_searchStringChanged(const QString&)));

    ui->gridCameras->setModel(m_resourceSearch);
}

QnCameraListDialog::~QnCameraListDialog()
{
}

void QnCameraListDialog::at_searchStringChanged(const QString& text)
{
    QString searchString = QString(lit("*%1*")).arg(text);
    m_resourceSearch->clearCriteria();
    m_resourceSearch->addCriterion(QnResourceCriterion(QRegExp(searchString, Qt::CaseInsensitive, QRegExp::Wildcard)));
}
