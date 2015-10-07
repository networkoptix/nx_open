#include "camera_list_dialog.h"
#include "ui_camera_list_dialog.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>
#include <QtCore/QMimeData>

#include <core/resource/resource_name.h>
#include <core/resource/device_dependent_strings.h>
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
    
    QnVirtualCameraResourceList cameras;
    for (int row = 0; row < m_resourceSearch->rowCount(); ++row) {
        QModelIndex index = m_resourceSearch->index(row, 0);
        if (!index.isValid())
            continue;
        if (QnVirtualCameraResourcePtr camera = index.data(Qn::ResourceRole).value<QnResourcePtr>().dynamicCast<QnVirtualCameraResource>())
            cameras << camera;
    }
    Q_ASSERT_X(cameras.size() == m_resourceSearch->rowCount(), Q_FUNC_INFO, "Make sure all found resources are cameras");

    const QString titleServerPart = m_model->server()
        ? QnDeviceDependentStrings::getDefaultNameFromSet(
                //: Devices List for Server (192.168.0.1)
                tr("Devices List for %1"),

                //: Cameras List for Server (192.168.0.1)
                tr("Cameras List for %1")
            ).arg(getFullResourceName(m_model->server(), true))
        : QnDeviceDependentStrings::getDefaultNameFromSet(
            tr("Devices List"),
            tr("Cameras List")
            );


    const QString titleCamerasPart = QnDeviceDependentStrings::getNameFromSet(
        QnCameraDeviceStringSet(
            tr("%n devices found",      nullptr, cameras.size()),
            tr("%n cameras found",      nullptr, cameras.size()),
            tr("%n IO modules found",   nullptr, cameras.size())
        ),
        cameras
     );

    const QString title = lit("%1 - %2").arg(titleServerPart).arg(titleCamerasPart);
    setWindowTitle(title);
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
        context()->menu()->trigger(Qn::CameraSettingsAction, QnActionParameters(resource));
}

void QnCameraListDialog::at_camerasView_customContextMenuRequested(const QPoint &) {
    QnResourceList resources;
    foreach(QModelIndex idx, ui->camerasView->selectionModel()->selectedRows())
        if (QnResourcePtr resource = idx.data(Qn::ResourceRole).value<QnResourcePtr>())
            resources.push_back(resource);


    QScopedPointer<QMenu> menu;
    if (!resources.isEmpty()) {
        QnActionParameters parameters(resources);
        parameters.setArgument(Qn::NodeTypeRole, Qn::ResourceNode);

        menu.reset(context()->menu()->newMenu(Qn::TreeScope, this, parameters, QnActionManager::DontReuseActions)); /* We'll be changing hotkeys, so we cannot reuse global actions. */
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
    QnGridWidgetHelper::exportToFile(ui->camerasView, this, tr("Export selected %1 to a file.").arg(getDefaultDeviceNameLower()));
}

void QnCameraListDialog::at_clipboardAction_triggered() {
    QnGridWidgetHelper::copyToClipboard(ui->camerasView);
}

