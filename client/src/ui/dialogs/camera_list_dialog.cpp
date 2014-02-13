#include "camera_list_dialog.h"
#include "ui_camera_list_dialog.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>
#include <QtCore/QMimeData>

#include <core/resource/resource_name.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <ui/models/camera_list_model.h>
#include <ui/workbench/workbench_context.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/actions/action_manager.h>
#include <ui/common/grid_widget_helper.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

QnCameraListDialog::QnCameraListDialog(QWidget *parent):
    QDialog(parent, Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowSystemMenuHint | Qt::WindowContextHelpButtonHint | Qt::WindowCloseButtonHint
#ifdef Q_OS_MAC
    | Qt::Tool
#endif
    ),
    QnWorkbenchContextAware(parent),
    ui(new Ui::CameraListDialog),
    m_model(new QnCameraListModel(this)),
    m_resourceSearch(new QnResourceSearchProxyModel(this))
{
    ui->setupUi(this);

  /*  QList<QnCameraListModel::Column> columns;
    columns << QnCameraListModel::RecordingColumn << QnCameraListModel::NameColumn << QnCameraListModel::VendorColumn << QnCameraListModel::ModelColumn <<
               QnCameraListModel::FirmwareColumn << QnCameraListModel::DriverColumn << QnCameraListModel::IpColumn << QnCameraListModel::UniqIdColumn << QnCameraListModel::ServerColumn;

    m_model->setColumns(columns);
    m_model->setResources(qnResPool->getAllEnabledCameras());
    */

    connect(m_model,  &QnCameraListModel::serverChanged, this, &QnCameraListDialog::at_modelChanged);
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
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);

    connect(m_clipboardAction,      SIGNAL(triggered()),                this, SLOT(at_copyToClipboard()));
    connect(m_exportAction,         SIGNAL(triggered()),                this, SLOT(at_exportAction()));
    connect(m_selectAllAction,      SIGNAL(triggered()),                ui->gridCameras, SLOT(selectAll()));

    ui->gridCameras->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    setHelpTopic(this, Qn::CameraList_Help);
}

QnCameraListDialog::~QnCameraListDialog() { }

void QnCameraListDialog::setServer(const QnMediaServerResourcePtr &server) {
    m_model->setServer(server);
}

QnMediaServerResourcePtr QnCameraListDialog::server() const {
    return m_model->server();
}

void QnCameraListDialog::at_searchStringChanged(const QString& text) {
    QString searchString = QString(lit("*%1*")).arg(text);
    m_resourceSearch->clearCriteria();
    m_resourceSearch->addCriterion(QnResourceCriterion(QRegExp(searchString, Qt::CaseInsensitive, QRegExp::Wildcard)));
}

void QnCameraListDialog::at_gridDoubleClicked(const QModelIndex &index) {
    if (!index.isValid())
        return;

    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if (resource)
        context()->menu()->trigger(Qn::OpenInCameraSettingsDialogAction, QnActionParameters(resource));
}

void QnCameraListDialog::at_customContextMenuRequested(const QPoint &pos) {
    Q_UNUSED(pos);

    QModelIndexList list = ui->gridCameras->selectionModel()->selectedRows();
    QnResourceList resList;
    foreach(QModelIndex idx, list)
    {
        QnResourcePtr resource = idx.data(Qn::ResourceRole).value<QnResourcePtr>();
        if (resource)
            resList << resource;
    }

    QMenu* menu = NULL;
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

void QnCameraListDialog::at_exportAction() {
    QnGridWidgetHelper::exportToFile(ui->gridCameras, this, tr("Export selected cameras to file"));
}

void QnCameraListDialog::at_copyToClipboard() {
    QnGridWidgetHelper::copyToClipboard(ui->gridCameras);
}

void QnCameraListDialog::at_modelChanged() {
    if (!m_model->server())
        setWindowTitle(tr("Camera List - %n camera(s) found", "", m_resourceSearch->rowCount()));
    else
        setWindowTitle(tr("Camera List for '%1' - %n camera(s) found", "", m_resourceSearch->rowCount()).arg(getFullResourceName(m_model->server(), true)));
}

