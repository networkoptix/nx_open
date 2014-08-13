#include "camera_list_dialog.h"
#include "ui_camera_list_dialog.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>
#include <QtCore/QMimeData>

#include <core/resource/resource_name.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <ui/models/camera_list_model.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/actions/action_manager.h>
#include <ui/common/grid_widget_helper.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/workbench/workbench_context.h>

QnCameraListDialog::QnCameraListDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::CameraListDialog),
    m_model(new QnCameraListModel(this)),
    m_resourceSearch(new QnResourceSearchProxyModel(this)),
    m_pendingWindowTitleUpdate(false)
{
    ui->setupUi(this);

  /*  QList<QnCameraListModel::Column> columns;
    columns << QnCameraListModel::RecordingColumn << QnCameraListModel::NameColumn << QnCameraListModel::VendorColumn << QnCameraListModel::ModelColumn <<
               QnCameraListModel::FirmwareColumn << QnCameraListModel::DriverColumn << QnCameraListModel::IpColumn << QnCameraListModel::UniqIdColumn << QnCameraListModel::ServerColumn;

    m_model->setColumns(columns);
    m_model->setResources(qnResPool->getAllEnabledCameras());
    */

    m_resourceSearch->setSourceModel(m_model);
    updateCriterion();

    connect(m_resourceSearch,   &QAbstractItemModel::rowsInserted,              this,   &QnCameraListDialog::updateWindowTitleLater);
    connect(m_resourceSearch,   &QAbstractItemModel::rowsRemoved,               this,   &QnCameraListDialog::updateWindowTitleLater);
    connect(m_resourceSearch,   &QAbstractItemModel::modelReset,                this,   &QnCameraListDialog::updateWindowTitleLater);
    connect(this,               &QnCameraListDialog::updateWindowTitleQueued,   this,   &QnCameraListDialog::updateWindowTitle, Qt::QueuedConnection);

    ui->camerasView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->camerasView->setModel(m_resourceSearch);
    connect(ui->filterEdit,     &QLineEdit::textChanged,                        this,   &QnCameraListDialog::updateCriterion);
    connect(ui->camerasView,    &QTableView::customContextMenuRequested,        this,   &QnCameraListDialog::at_camerasView_customContextMenuRequested);
    connect(ui->camerasView,    &QTableView::doubleClicked,                     this,   &QnCameraListDialog::at_camerasView_doubleClicked);


    m_clipboardAction   = new QAction(tr("Copy Selection to Clipboard"), this);
    m_clipboardAction->setShortcut(QKeySequence::Copy);
    ui->camerasView->addAction(m_clipboardAction);

    m_exportAction      = new QAction(tr("Export Selection to File..."), this);
    m_selectAllAction   = new QAction(tr("Select All"), this);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);

    connect(m_clipboardAction,  &QAction::triggered,                            this,   &QnCameraListDialog::at_clipboardAction_triggered);
    connect(m_exportAction,     &QAction::triggered,                            this,   &QnCameraListDialog::at_exportAction_triggered);
    connect(m_selectAllAction,  &QAction::triggered,                            ui->camerasView, &QTableView::selectAll);

    ui->camerasView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    setHelpTopic(this, Qn::CameraList_Help);
}

QnCameraListDialog::~QnCameraListDialog() { }

void QnCameraListDialog::setServer(const QnMediaServerResourcePtr &server) {
    m_model->setServer(server);
    // This fix is for the first time, the user open the camera list that doesn't see
    // the title of that dialog to indicate the camera numbers. I've noticed that the
    // message/signal that invoke this updateWindowTitle slot will not be triggered 
    // just by opening the dialog in the menu. This quick but not dirty fix just flush
    // window title whenever a serverResource is changed .
    updateWindowTitle();
}

QnMediaServerResourcePtr QnCameraListDialog::server() const {
    return m_model->server();
}

void QnCameraListDialog::updateWindowTitleLater() {
    if(m_pendingWindowTitleUpdate)
        return;

    m_pendingWindowTitleUpdate = true;
    updateWindowTitleQueued();
}

void QnCameraListDialog::updateWindowTitle() {
    m_pendingWindowTitleUpdate = false;
    
    if (!m_model->server())
        setWindowTitle(tr("Camera List - %n camera(s) found", "", m_resourceSearch->rowCount()));
    else
        setWindowTitle(tr("Camera List for '%1' - %n camera(s) found", "", m_resourceSearch->rowCount()).arg(getFullResourceName(m_model->server(), true)));
}

void QnCameraListDialog::updateCriterion() {
    QString text = ui->filterEdit->text();

    QString searchString = text.isEmpty()
        ? lit("*")
        : lit("*%1*").arg(text);
    m_resourceSearch->clearCriteria();
    m_resourceSearch->addCriterion(QnResourceCriterion(QRegExp(searchString, Qt::CaseInsensitive, QRegExp::Wildcard)));
    m_resourceSearch->addCriterion(QnResourceCriterion(Qn::desktop_camera, QnResourceProperty::flags, QnResourceCriterion::Reject, QnResourceCriterion::Next));
}

void QnCameraListDialog::at_camerasView_doubleClicked(const QModelIndex &index) {
    if (!index.isValid())
        return;

    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if (resource)
        context()->menu()->trigger(Qn::OpenInCameraSettingsDialogAction, QnActionParameters(resource));
}

void QnCameraListDialog::at_camerasView_customContextMenuRequested(const QPoint &) {
    QnResourceList resources;
    foreach(QModelIndex idx, ui->camerasView->selectionModel()->selectedRows())
        if (QnResourcePtr resource = idx.data(Qn::ResourceRole).value<QnResourcePtr>())
            resources.push_back(resource);

    QScopedPointer<QMenu> menu;
    if (!resources.isEmpty()) {
        menu.reset(context()->menu()->newMenu(Qn::TreeScope, this, resources, QnActionManager::DontReuseActions)); /* We'll be changing hotkeys, so we cannot reuse global actions. */
        foreach(QAction *action, menu->actions())
            action->setShortcut(QKeySequence());
    }

    if (menu) {
        menu->addSeparator();
    } else {
        menu.reset(new QMenu(this));
    }

    m_clipboardAction->setEnabled(ui->camerasView->selectionModel()->hasSelection());
    m_exportAction->setEnabled(ui->camerasView->selectionModel()->hasSelection());

    menu->addAction(m_selectAllAction);
    menu->addAction(m_exportAction);
    menu->addAction(m_clipboardAction);

    menu->exec(QCursor::pos());
}

void QnCameraListDialog::at_exportAction_triggered() {
    QnGridWidgetHelper::exportToFile(ui->camerasView, this, tr("Export selected cameras to file"));
}

void QnCameraListDialog::at_clipboardAction_triggered() {
    QnGridWidgetHelper::copyToClipboard(ui->camerasView);
}

