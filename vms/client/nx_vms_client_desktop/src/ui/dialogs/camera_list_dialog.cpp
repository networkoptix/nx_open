#include "camera_list_dialog.h"
#include "ui_camera_list_dialog.h"

#include <chrono>

#include <QtCore/QScopedPointer>
#include <QtWidgets/QMenu>

#include <common/common_module.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/camera_list_model.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/utils/table_export_helper.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workaround/hidpi_workarounds.h>

#include <nx/utils/pending_operation.h>
#include <nx/vms/client/desktop/common/widgets/item_view_auto_hider.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/resource_views/data/node_type.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>

using namespace std::chrono;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

struct QnCameraListDialog::Private
{
    QnCameraListModel* const model;
    QnResourceSearchProxyModel* const resourceSearch;
    const QScopedPointer<nx::utils::PendingOperation> updateTitle;
    QAction* const clipboardAction;
    QAction* const exportAction;
    QAction* const selectAllAction;

    Private(QnCameraListDialog* q):
        model(new QnCameraListModel(q)),
        resourceSearch(new QnResourceSearchProxyModel(q)),
        updateTitle(new nx::utils::PendingOperation()),
        clipboardAction(new QAction(tr("Copy Selection to Clipboard"), q)),
        exportAction(new QAction(tr("Export Selection to File..."), q)),
        selectAllAction(new QAction(tr("Select All"), q))
    {
        resourceSearch->setSourceModel(model);
        resourceSearch->setFilterCaseSensitivity(Qt::CaseInsensitive);

        clipboardAction->setShortcut(QKeySequence::Copy);
        selectAllAction->setShortcut(QKeySequence::SelectAll);

        updateTitle->setInterval(1ms);
        updateTitle->setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);
        updateTitle->setCallback([q]() { q->updateWindowTitle(); });

        connect(resourceSearch, &QAbstractItemModel::rowsInserted,
            updateTitle.data(), &nx::utils::PendingOperation::requestOperation);
        connect(resourceSearch, &QAbstractItemModel::rowsRemoved,
            updateTitle.data(), &nx::utils::PendingOperation::requestOperation);
        connect(resourceSearch, &QAbstractItemModel::modelReset,
            updateTitle.data(), &nx::utils::PendingOperation::requestOperation);
    }
};

QnCameraListDialog::QnCameraListDialog(QWidget* parent):
    base_type(parent),
    d(new Private(this)),
    ui(new Ui::CameraListDialog)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags()
        | Qt::WindowMaximizeButtonHint
        | Qt::MaximizeUsingFullscreenGeometryHint);

    const auto verticalScrollBar = new SnappedScrollBar(Qt::Vertical, ui->tableContainer);
    ui->camerasView->setVerticalScrollBar(verticalScrollBar->proxyScrollBar());
    const auto horizontalScrollBar = new SnappedScrollBar(Qt::Horizontal, ui->tableContainer);
    horizontalScrollBar->setUseMaximumSpace(true);
    horizontalScrollBar->setUseItemViewPaddingWhenVisible(true);
    ui->camerasView->setHorizontalScrollBar(horizontalScrollBar->proxyScrollBar());

    updateCriterion();

    ui->camerasView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->camerasView->setModel(d->resourceSearch);
    connect(ui->filterLineEdit, &SearchLineEdit::textChanged,
        this, &QnCameraListDialog::updateCriterion);
    connect(ui->camerasView, &QTableView::customContextMenuRequested,
        this, &QnCameraListDialog::at_camerasView_customContextMenuRequested);
    connect(ui->camerasView, &QTableView::doubleClicked,
        this, &QnCameraListDialog::at_camerasView_doubleClicked);

    connect(ui->addDeviceButton, &QPushButton::clicked, this,
        [this]
        {
            const auto parameters = action::Parameters(commonModule()->currentServer())
                .withArgument(Qn::ParentWidgetRole, QPointer<QWidget>(this));
            menu()->trigger(action::AddDeviceManuallyAction, parameters);
        });

    connect(d->clipboardAction, &QAction::triggered,
        this, &QnCameraListDialog::at_clipboardAction_triggered);
    connect(d->exportAction, &QAction::triggered,
        this, &QnCameraListDialog::at_exportAction_triggered);
    connect(d->selectAllAction, &QAction::triggered,
        ui->camerasView, &QTableView::selectAll);

    ui->camerasView->addAction(d->clipboardAction);
    ui->camerasView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->camerasView->horizontalHeader()->setSectionResizeMode(QnCameraListModel::NameColumn,
        QHeaderView::Interactive); //< Name may be of any length

    setHelpTopic(this, Qn::CameraList_Help);

    ItemViewAutoHider::create(ui->camerasView, tr("No cameras"));
}

QnCameraListDialog::~QnCameraListDialog()
{
}

void QnCameraListDialog::setServer(const QnMediaServerResourcePtr& server)
{
    d->model->setServer(server);
    // This fix is for the first time, the user open the camera list that doesn't see
    // the title of that dialog to indicate the camera numbers. I've noticed that the
    // message/signal that invoke this updateWindowTitle slot will not be triggered
    // just by opening the dialog in the menu. This quick but not dirty fix just flush
    // window title whenever a serverResource is changed .
    updateWindowTitle();
}

QnMediaServerResourcePtr QnCameraListDialog::server() const
{
    return d->model->server();
}

void QnCameraListDialog::updateWindowTitle()
{
    QnVirtualCameraResourceList cameras;
    for (int row = 0; row < d->resourceSearch->rowCount(); ++row)
    {
        const auto index = d->resourceSearch->index(row, 0);
        if (!index.isValid())
            continue;
        const auto camera = index.data(Qn::ResourceRole).value<QnResourcePtr>()
            .dynamicCast<QnVirtualCameraResource>();
        if (camera)
            cameras << camera;
    }

    NX_ASSERT(cameras.size() == d->resourceSearch->rowCount(),
        "Make sure all found resources are cameras");

    const QString titleServerPart =
        [this]()
        {
            if (server())
            {
                return QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(),
                    tr("Devices List for %1", "%1 will be substituted with a server name"),
                    tr("Cameras List for %1", "%1 will be substituted with a server name"))
                        .arg(QnResourceDisplayInfo(d->model->server()).toString(Qn::RI_WithUrl));
            }

            return QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(),
                tr("Devices List"), tr("Cameras List"));
        }();

    const QString titleCamerasPart = QnDeviceDependentStrings::getNameFromSet(resourcePool(),
        QnCameraDeviceStringSet(
            tr("%n devices found",      "", cameras.size()),
            tr("%n cameras found",      "", cameras.size()),
            tr("%n I/O modules found",  "", cameras.size())),
        cameras);

    const QString title = lit("%1 - %2").arg(titleServerPart).arg(titleCamerasPart);
    setWindowTitle(title);

    // TODO: #common Semantically misplaced. Required to update columns after filtering.
    ui->camerasView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
}

void QnCameraListDialog::updateCriterion()
{
    d->resourceSearch->setQuery(ui->filterLineEdit->text());
}

void QnCameraListDialog::at_camerasView_doubleClicked(const QModelIndex& index)
{
    if (!index.isValid())
        return;

    if (const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>())
        context()->menu()->trigger(action::CameraSettingsAction, resource);
}

void QnCameraListDialog::at_camerasView_customContextMenuRequested(const QPoint& /*point*/)
{
    QnResourceList resources;
    for (const auto& index: ui->camerasView->selectionModel()->selectedRows())
    {
        if (const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>())
            resources.push_back(resource);
    }

    QScopedPointer<QMenu> menu;
    if (!resources.isEmpty())
    {
        action::Parameters parameters(resources);
        parameters.setArgument(Qn::NodeTypeRole, ResourceTreeNodeType::resource);

        // We'll be changing hotkeys, so we cannot reuse global actions.
        menu.reset(context()->menu()->newMenu(action::TreeScope, nullptr,
            parameters, action::Manager::DontReuseActions));

        for (const auto action: menu->actions())
            action->setShortcut({});
    }

    if (menu)
        menu->addSeparator();
    else
        menu.reset(new QMenu(this));

    d->clipboardAction->setEnabled(ui->camerasView->selectionModel()->hasSelection());
    d->exportAction->setEnabled(ui->camerasView->selectionModel()->hasSelection());

    menu->addAction(d->selectAllAction);
    menu->addAction(d->exportAction);
    menu->addAction(d->clipboardAction);

    QnHiDpiWorkarounds::showMenu(menu.data(), QCursor::pos());
}

void QnCameraListDialog::at_exportAction_triggered()
{
    QnTableExportHelper::exportToFile(ui->camerasView, true, this,
        QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(),
            tr("Export selected devices to a file."),
            tr("Export selected cameras to a file.")));
}

void QnCameraListDialog::at_clipboardAction_triggered()
{
    QnTableExportHelper::copyToClipboard(ui->camerasView);
}
