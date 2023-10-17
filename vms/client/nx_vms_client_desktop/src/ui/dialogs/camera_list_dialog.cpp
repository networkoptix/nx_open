// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_list_dialog.h"
#include "ui_camera_list_dialog.h"

#include <chrono>

#include <QtCore/QScopedPointer>
#include <QtCore/QSortFilterProxyModel>
#include <QtWidgets/QMenu>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/pending_operation.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/desktop/common/widgets/item_view_auto_hider.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/models/camera_list_model.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/utils/table_export_helper.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_context.h>

using namespace std::chrono;
using namespace nx::vms::client::desktop;

struct QnCameraListDialog::Private
{
    QnCameraListModel* const model;
    QSortFilterProxyModel* const resourceSort;
    QnResourceSearchProxyModel* const resourceSearch;
    const QScopedPointer<nx::utils::PendingOperation> updateTitle;
    QAction* const clipboardAction;
    QAction* const exportAction;
    QAction* const selectAllAction;

    Private(QnCameraListDialog* q):
        model(new QnCameraListModel(q)),
        resourceSort(new QSortFilterProxyModel(q)),
        resourceSearch(new QnResourceSearchProxyModel(q)),
        updateTitle(new nx::utils::PendingOperation()),
        clipboardAction(new QAction(tr("Copy Selection to Clipboard"), q)),
        exportAction(new QAction(tr("Export Selection to File..."), q)),
        selectAllAction(new QAction(tr("Select All"), q))
    {
        resourceSort->setSourceModel(model);
        resourceSearch->setSourceModel(resourceSort);

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
            const auto parameters = menu::Parameters(system()->currentServer())
                .withArgument(Qn::ParentWidgetRole, QPointer<QWidget>(this));
            menu()->trigger(menu::AddDeviceManuallyAction, parameters);
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

    setHelpTopic(this, HelpTopic::Id::CameraList);

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
                return QnDeviceDependentStrings::getDefaultNameFromSet(system()->resourcePool(),
                    tr("Devices List for %1", "%1 will be substituted with a server name"),
                    tr("Cameras List for %1", "%1 will be substituted with a server name"))
                        .arg(QnResourceDisplayInfo(d->model->server()).toString(Qn::RI_WithUrl));
            }

            return QnDeviceDependentStrings::getDefaultNameFromSet(system()->resourcePool(),
                tr("Devices List"), tr("Cameras List"));
        }();

    const QString titleCamerasPart = QnDeviceDependentStrings::getNameFromSet(
        system()->resourcePool(),
        QnCameraDeviceStringSet(
            tr("%n devices found",      "", cameras.size()),
            tr("%n cameras found",      "", cameras.size()),
            tr("%n I/O modules found",  "", cameras.size())),
        cameras);

    const QString title = lit("%1 - %2").arg(titleServerPart).arg(titleCamerasPart);
    setWindowTitle(title);

    // TODO: #vkutin Semantically misplaced. Required to update columns after filtering.
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
        menu()->trigger(menu::CameraSettingsAction, resource);
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
        menu::Parameters parameters(resources);
        parameters.setArgument(Qn::NodeTypeRole, ResourceTree::NodeType::resource);

        // We'll be changing hotkeys, so we cannot reuse global actions.
        menu.reset(this->menu()->newMenu(
            menu::TableScope, this, parameters, menu::Manager::DontReuseActions));

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
    const auto caption = QnDeviceDependentStrings::getDefaultNameFromSet(system()->resourcePool(),
        tr("Export selected devices to a file."),
        tr("Export selected cameras to a file."));

    QnTableExportHelper::exportToFile(
        ui->camerasView->model(),
        ui->camerasView->selectionModel()->selectedIndexes(),
        this,
        caption);
}

void QnCameraListDialog::at_clipboardAction_triggered()
{
    QnTableExportHelper::copyToClipboard(
        ui->camerasView->model(),
        ui->camerasView->selectionModel()->selectedIndexes());
}
