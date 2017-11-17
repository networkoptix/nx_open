#include "camera_list_dialog.h"
#include "ui_camera_list_dialog.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>
#include <QtCore/QMimeData>

#include <common/common_module.h>

#include <core/resource/resource_display_info.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <ui/models/camera_list_model.h>
#include <ui/models/resource_search_proxy_model.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/utils/table_export_helper.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/widgets/common/item_view_auto_hider.h>
#include <ui/widgets/common/snapped_scrollbar.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workaround/hidpi_workarounds.h>

using namespace nx::client::desktop::ui;

QnCameraListDialog::QnCameraListDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::CameraListDialog),
    m_model(new QnCameraListModel(this)),
    m_resourceSearch(new QnResourceSearchProxyModel(this)),
    m_pendingWindowTitleUpdate(false)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags()
        | Qt::WindowMaximizeButtonHint
        | Qt::MaximizeUsingFullscreenGeometryHint);

    QnSnappedScrollBar* verticalScrollBar = new QnSnappedScrollBar(Qt::Vertical, this);
    ui->camerasView->setVerticalScrollBar(verticalScrollBar->proxyScrollBar());
    QnSnappedScrollBar* horizontalScrollBar = new QnSnappedScrollBar(Qt::Horizontal, this);
    horizontalScrollBar->setUseItemViewPaddingWhenVisible(true);
    ui->camerasView->setHorizontalScrollBar(horizontalScrollBar->proxyScrollBar());

    m_resourceSearch->setSourceModel(m_model);
    m_resourceSearch->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_resourceSearch->setFilterRole(Qn::ResourceSearchStringRole);

    updateCriterion();

    connect(m_resourceSearch,   &QAbstractItemModel::rowsInserted,              this,   &QnCameraListDialog::updateWindowTitleLater);
    connect(m_resourceSearch,   &QAbstractItemModel::rowsRemoved,               this,   &QnCameraListDialog::updateWindowTitleLater);
    connect(m_resourceSearch,   &QAbstractItemModel::modelReset,                this,   &QnCameraListDialog::updateWindowTitleLater);
    connect(this,               &QnCameraListDialog::updateWindowTitleQueued,   this,   &QnCameraListDialog::updateWindowTitle, Qt::QueuedConnection);

    ui->camerasView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->camerasView->setModel(m_resourceSearch);
    connect(ui->filterLineEdit, &QnSearchLineEdit::textChanged,                 this,   &QnCameraListDialog::updateCriterion);
    connect(ui->camerasView,    &QTableView::customContextMenuRequested,        this,   &QnCameraListDialog::at_camerasView_customContextMenuRequested);
    connect(ui->camerasView,    &QTableView::doubleClicked,                     this,   &QnCameraListDialog::at_camerasView_doubleClicked);

    connect(ui->addDeviceButton, &QPushButton::clicked, this,
        [this]
        {
            const auto parameters = action::Parameters(commonModule()->currentServer())
                .withArgument(Qn::ParentWidgetRole, QPointer<QWidget>(this));
            menu()->trigger(action::ServerAddCameraManuallyAction, parameters);
        });


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
    ui->camerasView->horizontalHeader()->setSectionResizeMode(QnCameraListModel::NameColumn,
        QHeaderView::Interactive); //< Name may be of any length

    setHelpTopic(this, Qn::CameraList_Help);

    QnItemViewAutoHider::create(ui->camerasView, tr("No cameras"));
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
    NX_ASSERT(cameras.size() == m_resourceSearch->rowCount(), Q_FUNC_INFO, "Make sure all found resources are cameras");

    const QString titleServerPart = m_model->server()
        ? QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
                //: Devices List for Server (192.168.0.1)
                tr("Devices List for %1"),

                //: Cameras List for Server (192.168.0.1)
                tr("Cameras List for %1")
            ).arg(QnResourceDisplayInfo(m_model->server()).toString(Qn::RI_WithUrl))
        : QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("Devices List"),
            tr("Cameras List")
            );


    const QString titleCamerasPart = QnDeviceDependentStrings::getNameFromSet(
        resourcePool(),
        QnCameraDeviceStringSet(
            tr("%n devices found",      "", cameras.size()),
            tr("%n cameras found",      "", cameras.size()),
            tr("%n I/O modules found",  "", cameras.size())
        ),
        cameras
     );

    const QString title = lit("%1 - %2").arg(titleServerPart).arg(titleCamerasPart);
    setWindowTitle(title);

    // TODO: #common Semantically misplaced. Required to update columns after filtering.
    ui->camerasView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
}

void QnCameraListDialog::updateCriterion()
{
    m_resourceSearch->setQuery(ui->filterLineEdit->text());
}

void QnCameraListDialog::at_camerasView_doubleClicked(const QModelIndex &index) {
    if (!index.isValid())
        return;

    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if (resource)
        context()->menu()->trigger(action::CameraSettingsAction, resource);
}

void QnCameraListDialog::at_camerasView_customContextMenuRequested(const QPoint &) {
    QnResourceList resources;
    foreach(QModelIndex idx, ui->camerasView->selectionModel()->selectedRows())
        if (QnResourcePtr resource = idx.data(Qn::ResourceRole).value<QnResourcePtr>())
            resources.push_back(resource);


    QScopedPointer<QMenu> menu;
    if (!resources.isEmpty()) {
        action::Parameters parameters(resources);
        parameters.setArgument(Qn::NodeTypeRole, Qn::ResourceNode);

        // We'll be changing hotkeys, so we cannot reuse global actions.
        menu.reset(context()->menu()->newMenu(action::TreeScope, nullptr,
            parameters, action::Manager::DontReuseActions));

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

    QnHiDpiWorkarounds::showMenu(menu.data(), QCursor::pos());
}

void QnCameraListDialog::at_exportAction_triggered() {
    QnTableExportHelper::exportToFile(ui->camerasView, true, this,
        QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("Export selected devices to a file."),
            tr("Export selected cameras to a file.")
            ));
}

void QnCameraListDialog::at_clipboardAction_triggered() {
    QnTableExportHelper::copyToClipboard(ui->camerasView);
}

