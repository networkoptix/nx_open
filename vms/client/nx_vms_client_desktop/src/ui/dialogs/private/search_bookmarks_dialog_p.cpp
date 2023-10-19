// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "search_bookmarks_dialog_p.h"
#include "ui_search_bookmarks_dialog.h"

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyledItemDelegate>

#include <common/common_module.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/resource/data_loaders/caching_camera_data_loader.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/common/delegates/customizable_item_delegate.h>
#include <nx/vms/client/desktop/common/widgets/item_view_auto_hider.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <recording/time_period.h>
#include <ui/common/read_only.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/models/search_bookmarks_model.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/synctime.h>

namespace {

constexpr int kTagColumnWidth = 240;

const QMap<int, std::pair<int, int>> kColumnsWidthLimit{
    {QnSearchBookmarksModel::kName, {80, 400}},
    {QnSearchBookmarksModel::kTags, {80, 320}}};

static const QColor kLight10Color = "#A5B7C0";
static const QColor kLight16Color = "#698796";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kLight10Color, "light10"}, {kLight16Color, "light16"}}},
    {QIcon::Active, {{kLight10Color, "light11"}, {kLight16Color, "light17"}}},
    {QIcon::Selected, {{kLight10Color, "light9"}}},
    {QnIcon::Error, {{kLight10Color, "red_l2"}}},
};

} // namespace

using namespace nx::vms::client::desktop;

QnSearchBookmarksDialogPrivate::QnSearchBookmarksDialogPrivate(
    const QString &filterText,
    qint64 utcStartTimeMs,
    qint64 utcFinishTimeMs,
    QDialog *owner)
    :
    QObject(owner),
    QnWorkbenchContextAware(owner),
    m_owner(owner),
    ui(new Ui::BookmarksLog()),
    m_model(new QnSearchBookmarksModel(this)),

    m_allCamerasChoosen(true),

    m_openInNewTabAction{new QAction(action(menu::OpenInNewTabAction)->text(), this)},
    m_editBookmarkAction{new QAction(action(menu::EditCameraBookmarkAction)->text(), this)},
    m_exportBookmarkAction{new QAction(action(menu::ExportBookmarkAction)->text(), this)},
    m_exportBookmarksAction{new QAction(action(menu::ExportBookmarksAction)->text(), this)},
    m_copyBookmarkTextAction{new QAction(action(menu::CopyBookmarkTextAction)->text(), this)},
    m_copyBookmarksTextAction{new QAction(action(menu::CopyBookmarksTextAction)->text(), this)},
    m_removeBookmarkAction{new QAction(action(menu::RemoveBookmarkAction)->text(), this)},
    m_removeBookmarksAction{new QAction(action(menu::RemoveBookmarksAction)->text(), this)},
    m_updatingParametersNow(false),
    m_columnResizeRequired(true),

    utcRangeStartMs(utcStartTimeMs),
    utcRangeEndMs(utcFinishTimeMs)
{
    ui->setupUi(m_owner);
    ui->refreshButton->setIcon(qnSkin->icon("text_buttons/reload_20.svg", kIconSubstitutions));
    ui->clearFilterButton->setIcon(
        qnSkin->icon("text_buttons/close_medium_20x20.svg", kIconSubstitutions));

    ui->gridBookmarks->setModel(m_model);

    const auto verticalScrollBar = new SnappedScrollBar(Qt::Vertical, ui->gridPage);
    const auto horizontalScrollBar = new SnappedScrollBar(Qt::Horizontal, ui->gridPage);
    ui->gridBookmarks->setVerticalScrollBar(verticalScrollBar->proxyScrollBar());
    ui->gridBookmarks->setHorizontalScrollBar(horizontalScrollBar->proxyScrollBar());
    horizontalScrollBar->setUseMaximumSpace(true);
    horizontalScrollBar->setUseItemViewPaddingWhenVisible(true);

    QnBookmarkSortOrder sortOrder = QnSearchBookmarksModel::defaultSortOrder();
    int column = QnSearchBookmarksModel::sortFieldToColumn(sortOrder.column);
    ui->gridBookmarks->horizontalHeader()->setSortIndicator(column, sortOrder.order);

    const auto updateFilterText = [this]()
    {
        auto text = ui->filterLineEdit->lineEdit()->text();
        m_model->setFilterText(text);
        applyModelChanges();
        //ui->clearFilterButton->setEnabled(!text.isEmpty());
    };

    enum { kUpdateFilterDelayMs = 200 };
    ui->filterLineEdit->setTextChangedSignalFilterMs(kUpdateFilterDelayMs);

    connect(ui->filterLineEdit, &SearchLineEdit::enterKeyPressed, this, updateFilterText);
    connect(ui->filterLineEdit, &SearchLineEdit::textChanged, this, updateFilterText);

    connect(ui->filterLineEdit, &SearchLineEdit::escKeyPressed, this, [this]()
    {
        ui->filterLineEdit->lineEdit()->setText(QString());
        m_model->setFilterText(QString());
        applyModelChanges();
    });

    connect(ui->dateRangeWidget, &QnDateRangeWidget::rangeChanged, this,
        [this](qint64 startTimeMs, qint64 endTimeMs)
        {
            if (m_updatingParametersNow)
                return;
            m_model->setRange(startTimeMs, endTimeMs);
            applyModelChanges();
        });

    connect(ui->clearFilterButton, &QPushButton::clicked, this,
        &QnSearchBookmarksDialogPrivate::reset);

    connect(ui->refreshButton, &QPushButton::clicked, this, &QnSearchBookmarksDialogPrivate::refresh);
    connect(ui->cameraButton, &QPushButton::clicked, this, &QnSearchBookmarksDialogPrivate::chooseCamera);

    connect(ui->gridBookmarks, &QTableView::customContextMenuRequested
        , this, &QnSearchBookmarksDialogPrivate::customContextMenuRequested);

    connect(ui->gridBookmarks, &QTableView::doubleClicked, this, [this](const QModelIndex & /* index */)
    {
        menu::Parameters params;
        QnTimePeriod window;
        if (!fillActionParameters(params, window))
            return;

        if (!menu()->canTrigger(menu::OpenInNewTabAction, params))
            return;

        openInNewLayout(params, window);
    });

    /* Context menu actions are connected in customContextMenuRequested() */

    setParameters(filterText, utcStartTimeMs, utcFinishTimeMs);

    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnSearchBookmarksDialogPrivate::reset);

    ui->filterLineEdit->lineEdit()->setPlaceholderText(
        tr("Search"));

    const auto grid = ui->gridBookmarks;
    ItemViewAutoHider::create(grid, tr("No bookmarks"));

    const auto header = grid->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->resizeSection(QnSearchBookmarksModel::kTags, kTagColumnWidth);

    connect(header, &QHeaderView::sectionResized,
        [header](int logicalIndex, int /*oldSize*/, int newSize)
        {
            if (!kColumnsWidthLimit.contains(logicalIndex))
                return;

            auto limits = kColumnsWidthLimit[logicalIndex];
            if (newSize < limits.first)
                header->resizeSection(logicalIndex, limits.first);
            else if (newSize > limits.second)
                header->resizeSection(logicalIndex, limits.second);
        });

    auto boldItemDelegate = new CustomizableItemDelegate(this);
    boldItemDelegate->setCustomInitStyleOption(
        [](QStyleOptionViewItem* option, const QModelIndex& /*index*/)
        {
            option->font.setBold(true);
        });

    grid->setItemDelegateForColumn(QnSearchBookmarksModel::kCamera, boldItemDelegate);

    auto nonFoundCreatorItemDelegate = new CustomizableItemDelegate(this);
    nonFoundCreatorItemDelegate->setCustomInitStyleOption(
        [](QStyleOptionViewItem* option, const QModelIndex& /*index*/)
        {
            if (option->text.isEmpty())
            {
                option->text = tr("N/A");
                option->palette.setColor(
                    QPalette::Text, nx::vms::client::core::colorTheme()->color("light16"));
                option->icon = QIcon();
                option->decorationSize = QSize();
            }
        });

    grid->setItemDelegateForColumn(QnSearchBookmarksModel::kCreator, nonFoundCreatorItemDelegate);
    grid->setStyleSheet(lit("QTableView::item { padding-right: 24px }"));

    connect(m_model, &QAbstractItemModel::modelAboutToBeReset, this,
        [this]() { ui->stackedWidget->setCurrentWidget(ui->progressPage); });

    connect(m_model, &QAbstractItemModel::modelReset,
        [this, grid]()
        {
            ui->stackedWidget->setCurrentWidget(ui->gridPage);
            if (m_columnResizeRequired && m_model->rowCount() > 0)
            {
                for (int columnIdx = 0; columnIdx < QnSearchBookmarksModel::kColumnsCount;
                     columnIdx++)
                {
                    grid->resizeColumnToContents(columnIdx);
                }
                m_columnResizeRequired = false;
            }
        });
    ui->stackedWidget->setCurrentWidget(ui->progressPage);
}

QnSearchBookmarksDialogPrivate::~QnSearchBookmarksDialogPrivate()
{
}

void QnSearchBookmarksDialogPrivate::applyModelChanges()
{
    if (!m_updatingParametersNow)
        m_model->applyFilter();
}

void QnSearchBookmarksDialogPrivate::reset()
{
    setParameters(QString(), utcRangeStartMs, utcRangeEndMs);
}

void QnSearchBookmarksDialogPrivate::setParameters(const QString &filterText
    , qint64 utcStartTimeMs
    , qint64 utcFinishTimeMs)
{
    {
        QScopedValueRollback<bool> guard(m_updatingParametersNow, true);
        m_columnResizeRequired = true;
        resetToAllAvailableCameras();
        ui->filterLineEdit->lineEdit()->setText(filterText);
        ui->dateRangeWidget->setRange(utcStartTimeMs, utcFinishTimeMs);
    }

    refresh();
}

bool QnSearchBookmarksDialogPrivate::fillActionParameters(menu::Parameters &params, QnTimePeriod &window)
{
    auto bookmarkFromIndex = [this](const QModelIndex &index) -> QnCameraBookmark
    {
        if (!index.isValid())
            return QnCameraBookmark();

        const auto bookmark = m_model->data(index, Qn::CameraBookmarkRole).value<QnCameraBookmark>();
        if (!bookmark.isValid())
            return QnCameraBookmark();

        if (!availableCameraById(bookmark.cameraId))
            return QnCameraBookmark();

        return bookmark;
    };


    params = menu::Parameters();

    QModelIndexList selection = ui->gridBookmarks->selectionModel()->selectedRows();
    QSet<int> selectedRows;
    for (const QModelIndex &index: selection)
        selectedRows << index.row();

    /* Update selection - add current item if we have clicked on not selected item with Ctrl or Shift. */
    const auto currentIndex = ui->gridBookmarks->currentIndex();
    if (currentIndex.isValid() && !selectedRows.contains(currentIndex.row()))
    {
        const int row = currentIndex.row();
        QItemSelection selectionItem(currentIndex.sibling(row, 0), currentIndex.sibling(row, QnSearchBookmarksModel::kColumnsCount - 1));

        ui->gridBookmarks->selectionModel()->select(selectionItem, QItemSelectionModel::Select);
        selection << currentIndex;
    }

    QnCameraBookmarkList bookmarks;
    QnResourceList resourcesList;
    QSet<QnUuid> bookmarkIds;   /*< Make sure we will have no duplicates in all cases. */
    for (const QModelIndex &index: selection)
    {
        const auto bookmark = bookmarkFromIndex(index);

        /* Add invalid bookmarks, user must be able to delete them. */
        if (!bookmark.isValid())
            continue;

        if (bookmarkIds.contains(bookmark.guid))
            continue;

        bookmarkIds << bookmark.guid;
        bookmarks << bookmark;
        if (auto camera = availableCameraById(bookmark.cameraId))
            resourcesList << camera;
    }
    params.setResources(resourcesList);
    if (bookmarks.isEmpty())
        return false;
    else
        params.setArgument(Qn::CameraBookmarkListRole, bookmarks);

    if (bookmarks.size() != 1)
        return true;

    QnCameraBookmark currentBookmark = bookmarks.first();
    /* User should not be able to export or open invalid bookmark. */
    if (!currentBookmark.isValid())
        return true;

    params.setArgument(Qn::CameraBookmarkRole, currentBookmark);
    window = QnTimePeriod(currentBookmark.startTimeMs, currentBookmark.durationMs);
    params.setArgument(Qn::TimePeriodRole, window);
    params.setArgument(Qn::ItemTimeRole, window.startTimeMs);

    return true;
}

void QnSearchBookmarksDialogPrivate::openInNewLayout(const menu::Parameters &params, const QnTimePeriod &window)
{
    const auto setFirstLayoutItemPeriod = [this](const QnTimePeriod &window
        , Qn::ItemDataRole role)
    {
        const auto layout = workbench()->currentLayout();
        const auto items = layout->items();
        if (items.empty())
            return;

        const auto item = *items.begin();
        item->setData(role, window);
    };

    menu()->trigger(menu::OpenInNewTabAction, params);

    static const qint64 kMinOffset = 30 * 1000;
    const auto offset = std::max(window.durationMs, kMinOffset);

    const QnTimePeriod extendedWindow(window.startTimeMs - offset
        , window.durationMs + offset * 2);
    setFirstLayoutItemPeriod(extendedWindow, Qn::ItemSliderWindowRole);

    menu()->trigger(menu::BookmarksTabAction);
}

void QnSearchBookmarksDialogPrivate::cancelUpdateOperation()
{
    m_model->cancelUpdateOperation();
}

void QnSearchBookmarksDialogPrivate::refresh()
{
    {
        QScopedValueRollback<bool> guard(m_updatingParametersNow, true);
        m_model->setFilterText(ui->filterLineEdit->lineEdit()->text());
        m_model->setRange(ui->dateRangeWidget->startTimeMs(), ui->dateRangeWidget->endTimeMs());
    }

    applyModelChanges();
}

QnVirtualCameraResourceList QnSearchBookmarksDialogPrivate::availableCameras() const
{
    return resourcePool()->getAllCameras(QnResourcePtr(), true).filtered(
        [this](const QnVirtualCameraResourcePtr& camera)
        {
            const auto& accessController = systemContext()->accessController();
            return accessController->hasPermissions(camera, Qn::ViewContentPermission)
                || accessController->hasPermissions(camera, Qn::ViewBookmarksPermission);
        }
    );
}

QnVirtualCameraResourcePtr QnSearchBookmarksDialogPrivate::availableCameraById(
    const QnUuid& cameraId) const
{
    const auto cameras = availableCameras();

    const auto it = std::find_if(cameras.begin(), cameras.end(),
        [cameraId](const auto& camera) { return camera->getId() == cameraId; });

    return it == cameras.end() ? QnVirtualCameraResourcePtr() : *it;
}

void QnSearchBookmarksDialogPrivate::resetToAllAvailableCameras()
{
    setCameras(QnVirtualCameraResourceList());
}

void QnSearchBookmarksDialogPrivate::setCameras(const QnVirtualCameraResourceList &cameras)
{
    m_allCamerasChoosen = cameras.empty();
    const auto &correctCameras = (m_allCamerasChoosen
        ? availableCameras() : cameras);

    std::set<QnUuid> cameraIds;
    for (const auto& camera: correctCameras)
        cameraIds.insert(camera->getId());
    m_model->setCameras(cameraIds);

    if (m_allCamerasChoosen)
        ui->cameraButton->selectAll();
    else
        ui->cameraButton->selectDevices(cameras);
}

void QnSearchBookmarksDialogPrivate::chooseCamera()
{
    QnUuidSet cameraIds;
    if (!m_allCamerasChoosen)
    {
        for (auto id: m_model->cameras())
            cameraIds << id;
    }

    auto dialogAccepted = CameraSelectionDialog::selectCameras
        <CameraSelectionDialog::DummyPolicy>(cameraIds, m_owner);

    if (dialogAccepted)
    {
        setCameras(resourcePool()->getResourcesByIds<QnVirtualCameraResource>(cameraIds));
        m_model->applyFilter();
    }
}

void QnSearchBookmarksDialogPrivate::customContextMenuRequested()
{
    /* Fill action parameters: */
    menu::Parameters params;
    QnTimePeriod window;
    if (!fillActionParameters(params, window))
        return;

    /* Create menu object: */
    auto newMenu = new QMenu(m_owner);

    /* Add suitable actions: */
    const auto addActionToMenu =
        [this, newMenu, params](menu::IDType id, QAction *action)
        {
            if (menu()->canTrigger(id, params))
                newMenu->addAction(action);
        };

    const auto addSeparatorToMenu =
        [newMenu]()
        {
            if (!newMenu->isEmpty() && !newMenu->actions().last()->isSeparator())
                newMenu->addSeparator();
        };

    addActionToMenu(menu::OpenInNewTabAction, m_openInNewTabAction);
    addSeparatorToMenu();
    addActionToMenu(menu::EditCameraBookmarkAction, m_editBookmarkAction);
    addActionToMenu(menu::ExportBookmarkAction, m_exportBookmarkAction);
    addActionToMenu(menu::ExportBookmarksAction, m_exportBookmarksAction);
    addActionToMenu(menu::CopyBookmarkTextAction, m_copyBookmarkTextAction);
    addActionToMenu(menu::CopyBookmarksTextAction, m_copyBookmarksTextAction);
    addSeparatorToMenu();
    addActionToMenu(menu::RemoveBookmarkAction, m_removeBookmarkAction);
    addActionToMenu(menu::RemoveBookmarksAction, m_removeBookmarksAction);

    /* Connect action signal handlers: */
    connect(m_openInNewTabAction, &QAction::triggered, this,
        [this, params, window]
        {
            openInNewLayout(params, window);
        });

    connect(m_editBookmarkAction, &QAction::triggered, this,
        [this, params]
        {
            menu()->triggerIfPossible(menu::EditCameraBookmarkAction, params);
        });

    connect(m_removeBookmarkAction, &QAction::triggered, this,
        [this, params]
        {
            const auto parentWidget = QPointer<QWidget>(m_owner);
            menu()->triggerIfPossible(menu::RemoveBookmarkAction,
                menu::Parameters(params).withArgument(Qn::ParentWidgetRole, parentWidget));
        });

    connect(m_removeBookmarksAction, &QAction::triggered, this,
        [this, params]
        {
            const auto parentWidget = QPointer<QWidget>(m_owner);
            menu()->triggerIfPossible(menu::RemoveBookmarksAction,
                menu::Parameters(params).withArgument(Qn::ParentWidgetRole, parentWidget));
        });

    connect(m_exportBookmarkAction, &QAction::triggered, this,
        [this, params]
        {
            menu()->triggerIfPossible(menu::ExportBookmarkAction, params);
        });

    connect(m_exportBookmarksAction, &QAction::triggered, this,
        [this, params]
        {
            menu()->triggerIfPossible(menu::ExportBookmarksAction, params);
        });

    connect(m_copyBookmarkTextAction, &QAction::triggered, ui->gridBookmarks,
        &CopyToClipboardTableView::copySelectedToClipboard);
    connect(m_copyBookmarksTextAction, &QAction::triggered, ui->gridBookmarks,
        &CopyToClipboardTableView::copySelectedToClipboard);

    /* Execute popup menu: */
    newMenu->exec(QCursor::pos());
    newMenu->deleteLater();

    /* Disconnect action signal handlers: */
    m_openInNewTabAction->disconnect(this);
    m_editBookmarkAction->disconnect(this);
    m_removeBookmarksAction->disconnect(this);
    m_removeBookmarkAction->disconnect(this);
    m_exportBookmarksAction->disconnect(this);
    m_exportBookmarkAction->disconnect(this);
    m_copyBookmarkTextAction->disconnect(this);
    m_copyBookmarksTextAction->disconnect(this);
}
