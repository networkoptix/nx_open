#include "camera_list_dialog.h"
#include "ui_camera_list_dialog.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>
#include <QtCore/QMimeData>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <ui/models/camera_list_model.h>
#include <ui/workbench/workbench_context.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/actions/action_manager.h>
#include <ui/common/grid_widget_helper.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

QnCameraListDialog::QnCameraListDialog(QWidget *parent, QnWorkbenchContext *context):
    QDialog(parent, Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowSystemMenuHint | Qt::WindowContextHelpButtonHint | Qt::WindowCloseButtonHint | Qt::Tool),
    QnWorkbenchContextAware(parent, context),
    ui(new Ui::CameraListDialog)
{
    ui->setupUi(this);

    m_model = new QnCameraListModel(context);
    connect(qnResPool,  SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resPool_resourceRemoved(const QnResourcePtr &)));
    connect(qnResPool,  SIGNAL(resourceAdded(const QnResourcePtr &)), this,   SLOT(at_resPool_resourceAdded(const QnResourcePtr &)));

    QList<QnCameraListModel::Column> columns;
    columns << QnCameraListModel::RecordingColumn << QnCameraListModel::NameColumn << QnCameraListModel::VendorColumn << QnCameraListModel::ModelColumn <<
               QnCameraListModel::FirmwareColumn << QnCameraListModel::DriverColumn << QnCameraListModel::IPColumn << QnCameraListModel::UniqIdColumn << QnCameraListModel::ServerColumn;

    m_model->setColumns(columns);

    m_resourceSearch = new QnResourceSearchProxyModel(this);
    connect(m_resourceSearch,  SIGNAL(criteriaChanged()), this, SLOT(at_modelChanged()) );
    connect(m_resourceSearch,  SIGNAL(modelReset()), this, SLOT(at_modelChanged()) );
    m_resourceSearch->setSourceModel(m_model);
    m_resourceSearch->addCriterion(QnResourceCriterion(QRegExp(lit("*"),Qt::CaseInsensitive, QRegExp::Wildcard)));

    connect(ui->SearchString, SIGNAL(textChanged(const QString &)), this, SLOT(at_searchStringChanged(const QString &)));
    connect(ui->gridCameras,  SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(at_customContextMenuRequested(const QPoint &)));
    connect(ui->gridCameras,  SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(at_gridDoubleClicked(const QModelIndex &)));

    ui->gridCameras->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->gridCameras->setModel(m_resourceSearch);

    m_clipboardAction   = new QAction(tr("Copy Selection to Clipboard"), this);
    m_clipboardAction->setShortcut(QKeySequence::Copy);
    ui->gridCameras->addAction(m_clipboardAction);

    m_exportAction      = new QAction(tr("Export Selection to File..."), this);
    m_selectAllAction   = new QAction(tr("Select All"), this);
    m_selectAllAction->setShortcut(Qt::CTRL + Qt::Key_A);

    connect(m_clipboardAction,      SIGNAL(triggered()),                this, SLOT(at_copyToClipboard()));
    connect(m_exportAction,         SIGNAL(triggered()),                this, SLOT(at_exportAction()));
    connect(m_selectAllAction,      SIGNAL(triggered()),                this, SLOT(at_selectAllAction()));

    ui->gridCameras->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    setHelpTopic(this, Qn::CameraList_Help);
}

QnCameraListDialog::~QnCameraListDialog()
{
}

void QnCameraListDialog::setServer(const QnMediaServerResourcePtr &server)
{
    if(m_server == server)
        return;

    m_server = server;
    m_model->setResources(qnResPool->getAllEnabledCameras(m_server));
}

const QnMediaServerResourcePtr &QnCameraListDialog::server() const 
{
    return m_server;
}

void QnCameraListDialog::at_searchStringChanged(const QString& text)
{
    QString searchString = QString(lit("*%1*")).arg(text);
    m_resourceSearch->clearCriteria();
    m_resourceSearch->addCriterion(QnResourceCriterion(QRegExp(searchString, Qt::CaseInsensitive, QRegExp::Wildcard)));
}

void QnCameraListDialog::at_gridDoubleClicked(const QModelIndex &index)
{
    if (index.isValid())
    {
        QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        if (resource)
            context()->menu()->trigger(Qn::OpenInCameraSettingsDialogAction, QnActionParameters(resource));
    }
}

void QnCameraListDialog::at_customContextMenuRequested(const QPoint &)
{
    QModelIndexList list = ui->gridCameras->selectionModel()->selectedRows();
    QnResourceList resList;
    foreach(QModelIndex idx, list)
    {
        QnResourcePtr resource = idx.data(Qn::ResourceRole).value<QnResourcePtr>();
        if (resource)
            resList << resource;
    }

    QMenu* menu = 0;
    QnActionManager* manager = context()->menu();

    if (!resList.isEmpty()) {
        menu = manager->newMenu(Qn::TreeScope, this, QnActionParameters(resList));
        foreach(QAction* action, menu->actions())
            action->setShortcut(QKeySequence());
    }

    if (menu)
        menu->addSeparator();
    else
        menu = new QMenu(this);

    m_clipboardAction->setEnabled(ui->gridCameras->selectionModel()->hasSelection());
    m_exportAction->setEnabled(ui->gridCameras->selectionModel()->hasSelection());

    menu->addAction(m_selectAllAction);
    menu->addAction(m_exportAction);
    menu->addAction(m_clipboardAction);

    menu->exec(QCursor::pos());
    menu->deleteLater();
}

void QnCameraListDialog::at_selectAllAction()
{
    ui->gridCameras->selectAll();
}

void QnCameraListDialog::at_exportAction()
{
    QnGridWidgetHelper(context()).exportToFile(ui->gridCameras, tr("Export selected cameras to file"));
}

void QnCameraListDialog::at_copyToClipboard()
{
    QnGridWidgetHelper(context()).copyToClipboard(ui->gridCameras);
}

void QnCameraListDialog::at_modelChanged()
{
    if (!m_server)
        setWindowTitle(tr("Camera List - %n camera(s) found", "", m_resourceSearch->rowCount()));
    else
        setWindowTitle(tr("Camera List for media server '%1' - %n camera(s) found", "", m_resourceSearch->rowCount()).arg(QUrl(m_server->getUrl()).host()));
}

void QnCameraListDialog::at_resPool_resourceRemoved(const QnResourcePtr & resource)
{
    m_model->removeResource(resource);
}

void QnCameraListDialog::at_resPool_resourceAdded(const QnResourcePtr & resource)
{
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (camera && (!m_server || camera->getParentId() == m_server->getId()))
        m_model->addResource(camera);
}

