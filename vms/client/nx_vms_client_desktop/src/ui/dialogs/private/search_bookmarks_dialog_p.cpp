#include "search_bookmarks_dialog_p.h"

#include <QtWidgets/QMenu>
#include <QtWidgets/QStyledItemDelegate>

#include "ui_search_bookmarks_dialog.h"

#include <nx/vms/client/desktop/ini.h>
#include <client/client_settings.h>
#include <common/common_module.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <recording/time_period.h>
#include <ui/common/read_only.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/models/search_bookmarks_model.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_access_controller.h>
#include <utils/common/synctime.h>

#include <nx/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/common/delegates/customizable_item_delegate.h>
#include <nx/vms/client/desktop/common/widgets/item_view_auto_hider.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

QnSearchBookmarksDialogPrivate::QnSearchBookmarksDialogPrivate(const QString &filterText
    , qint64 utcStartTimeMs
    , qint64 utcFinishTimeMs
    , QDialog *owner)

    : QObject(owner)
    , QnWorkbenchContextAware(owner)
    , m_owner(owner)
    , m_ui(new Ui::BookmarksLog())
    , m_model(new QnSearchBookmarksModel(this))

    , m_allCamerasChoosen(true)

    , m_openInNewTabAction      (new QAction(action(action::OpenInNewTabAction)->text(), this))
    , m_editBookmarkAction      (new QAction(action(action::EditCameraBookmarkAction)->text(), this))
    , m_exportBookmarkAction    (new QAction(tr("Export Bookmark..."), this))
    , m_exportBookmarksAction   (new QAction(tr("Export Bookmarks..."), this))
    , m_removeBookmarksAction   (new QAction(action(action::RemoveBookmarksAction)->text(), this))
    , m_updatingParametersNow(false)

    , utcRangeStartMs(utcStartTimeMs)
    , utcRangeEndMs(utcFinishTimeMs)
{
    m_ui->setupUi(m_owner);
    m_ui->refreshButton->setIcon(qnSkin->icon("text_buttons/refresh.png"));
    m_ui->clearFilterButton->setIcon(qnSkin->icon("text_buttons/clear.png"));

    m_ui->gridBookmarks->setModel(m_model);

    QnBookmarkSortOrder sortOrder = QnSearchBookmarksModel::defaultSortOrder();
    int column = QnSearchBookmarksModel::sortFieldToColumn(sortOrder.column);
    m_ui->gridBookmarks->horizontalHeader()->setSortIndicator(column, sortOrder.order);

    const auto updateFilterText = [this]()
    {
        auto text = m_ui->filterLineEdit->lineEdit()->text();
        m_model->setFilterText(text);
        applyModelChanges();
        //m_ui->clearFilterButton->setEnabled(!text.isEmpty());
    };

    enum { kUpdateFilterDelayMs = 200 };
    m_ui->filterLineEdit->setTextChangedSignalFilterMs(kUpdateFilterDelayMs);

    connect(m_ui->filterLineEdit, &SearchLineEdit::enterKeyPressed, this, updateFilterText);
    connect(m_ui->filterLineEdit, &SearchLineEdit::textChanged, this, updateFilterText);

    connect(m_ui->filterLineEdit, &SearchLineEdit::escKeyPressed, this, [this]()
    {
        m_ui->filterLineEdit->lineEdit()->setText(QString());
        m_model->setFilterText(QString());
        applyModelChanges();
    });

    connect(m_ui->dateRangeWidget, &QnDateRangeWidget::rangeChanged, this,
        [this](qint64 startTimeMs, qint64 endTimeMs)
        {
            if (m_updatingParametersNow)
                return;
            m_model->setRange(startTimeMs, endTimeMs);
            applyModelChanges();
        });

    connect(m_ui->clearFilterButton, &QPushButton::clicked, this,
        &QnSearchBookmarksDialogPrivate::reset);

    connect(m_ui->refreshButton, &QPushButton::clicked, this, &QnSearchBookmarksDialogPrivate::refresh);
    connect(m_ui->cameraButton, &QPushButton::clicked, this, &QnSearchBookmarksDialogPrivate::chooseCamera);

    connect(m_ui->gridBookmarks, &QTableView::customContextMenuRequested
        , this, &QnSearchBookmarksDialogPrivate::customContextMenuRequested);

    connect(m_ui->gridBookmarks, &QTableView::doubleClicked, this, [this](const QModelIndex & /* index */)
    {
        action::Parameters params;
        QnTimePeriod window;
        if (!fillActionParameters(params, window))
            return;

        if (!menu()->canTrigger(action::OpenInNewTabAction, params))
            return;

        openInNewLayout(params, window);
    });

    /* Context menu actions are connected in customContextMenuRequested() */

    setParameters(filterText, utcStartTimeMs, utcFinishTimeMs);

    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnSearchBookmarksDialogPrivate::reset);

    m_ui->filterLineEdit->lineEdit()->setPlaceholderText(
        tr("Search"));

    const auto grid = m_ui->gridBookmarks;
    ItemViewAutoHider::create(grid, tr("No bookmarks"));

    const auto header = grid->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(QnSearchBookmarksModel::kTags, QHeaderView::Stretch);

    auto boldItemDelegate = new CustomizableItemDelegate(this);
    boldItemDelegate->setCustomInitStyleOption(
        [](QStyleOptionViewItem* option, const QModelIndex& /*index*/)
        {
            option->font.setBold(true);
        });

    grid->setItemDelegateForColumn(QnSearchBookmarksModel::kCamera, boldItemDelegate);
    grid->setStyleSheet(lit("QTableView::item { padding-right: 24px }"));

    connect(m_model, &QAbstractItemModel::modelAboutToBeReset, this,
        [this]() { m_ui->stackedWidget->setCurrentWidget(m_ui->progressPage); });

    connect(m_model, &QAbstractItemModel::modelReset, this,
        [this]() { m_ui->stackedWidget->setCurrentWidget(m_ui->gridPage); });
    m_ui->stackedWidget->setCurrentWidget(m_ui->progressPage);
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

        resetToAllAvailableCameras();
        m_ui->filterLineEdit->lineEdit()->setText(filterText);
        m_ui->dateRangeWidget->setRange(utcStartTimeMs, utcFinishTimeMs);
    }

    refresh();
}

bool QnSearchBookmarksDialogPrivate::fillActionParameters(action::Parameters &params, QnTimePeriod &window)
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


    params = action::Parameters();

    QModelIndexList selection = m_ui->gridBookmarks->selectionModel()->selectedRows();
    QSet<int> selectedRows;
    for (const QModelIndex &index: selection)
        selectedRows << index.row();

    /* Update selection - add current item if we have clicked on not selected item with Ctrl or Shift. */
    const auto currentIndex = m_ui->gridBookmarks->currentIndex();
    if (currentIndex.isValid() && !selectedRows.contains(currentIndex.row()))
    {
        const int row = currentIndex.row();
        QItemSelection selectionItem(currentIndex.sibling(row, 0), currentIndex.sibling(row, QnSearchBookmarksModel::kColumnsCount - 1));

        m_ui->gridBookmarks->selectionModel()->select(selectionItem, QItemSelectionModel::Select);
        selection << currentIndex;
    }

    QnCameraBookmarkList bookmarks;
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
    }
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

    auto camera = availableCameraById(currentBookmark.cameraId);
    if (camera)
        params.setResources(QnResourceList() << camera);

    window = QnTimePeriod(currentBookmark.startTimeMs, currentBookmark.durationMs);
    params.setArgument(Qn::TimePeriodRole, window);
    params.setArgument(Qn::ItemTimeRole, window.startTimeMs);

    return true;
}

void QnSearchBookmarksDialogPrivate::openInNewLayout(const action::Parameters &params, const QnTimePeriod &window)
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

    menu()->trigger(action::OpenInNewTabAction, params);

    static const qint64 kMinOffset = 30 * 1000;
    const auto offset = std::max(window.durationMs, kMinOffset);

    const QnTimePeriod extendedWindow(window.startTimeMs - offset
        , window.durationMs + offset * 2);
    setFirstLayoutItemPeriod(extendedWindow, Qn::ItemSliderWindowRole);

    menu()->trigger(action::BookmarksModeAction);
}

void QnSearchBookmarksDialogPrivate::cancelUpdateOperation()
{
    m_model->cancelUpdateOperation();
}

void QnSearchBookmarksDialogPrivate::refresh()
{
    {
        QScopedValueRollback<bool> guard(m_updatingParametersNow, true);
        m_model->setFilterText(m_ui->filterLineEdit->lineEdit()->text());
        m_model->setRange(m_ui->dateRangeWidget->startTimeMs(), m_ui->dateRangeWidget->endTimeMs());
    }

    applyModelChanges();
}

QnVirtualCameraResourceList QnSearchBookmarksDialogPrivate::availableCameras() const
{
    return resourcePool()->getAllCameras(QnResourcePtr(), true).filtered(
        [this](const QnVirtualCameraResourcePtr& camera)
        {
            return accessController()->hasPermissions(camera, Qn::ViewContentPermission);
        }
    );
}

QnVirtualCameraResourcePtr QnSearchBookmarksDialogPrivate::availableCameraById(
    const QnUuid& cameraId) const
{
    const auto cameras = availableCameras();

    const auto it = std::find_if(cameras.begin(), cameras.end(),
        [this, cameraId](const QnVirtualCameraResourcePtr& camera)
        {
            return camera->getId() == cameraId;
        });

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

    m_model->setCameras(correctCameras);

    if (m_allCamerasChoosen)
        m_ui->cameraButton->selectAll();
    else
        m_ui->cameraButton->selectDevices(cameras);
}

void QnSearchBookmarksDialogPrivate::chooseCamera()
{
    QnUuidSet cameraIds;
    if (!m_allCamerasChoosen)
    {
        for (auto camera: m_model->cameras())
            cameraIds << camera->getId();
    }

    auto dialogAccepted = CameraSelectionDialog::selectCameras
        <CameraSelectionDialog::DummyPolicy>(cameraIds, m_owner);

    if (dialogAccepted)
    {
        setCameras(resourcePool()->getResourcesByIds<QnVirtualCameraResource>(cameraIds));
        m_model->applyFilter();
    }
}

bool QnSearchBookmarksDialogPrivate::currentUserHasAllCameras()
{
    return accessController()->hasGlobalPermission(GlobalPermission::accessAllMedia);
}

void QnSearchBookmarksDialogPrivate::customContextMenuRequested()
{
    /* Fill action parameters: */
    action::Parameters params;
    QnTimePeriod window;
    if (!fillActionParameters(params, window))
        return;

    /* Create menu object: */
    auto newMenu = new QMenu();

    /* Add suitable actions: */
    const auto addActionToMenu =
        [this, newMenu, params](action::IDType id, QAction *action)
        {
            if (menu()->canTrigger(id, params))
                newMenu->addAction(action);
        };

    addActionToMenu(action::OpenInNewTabAction, m_openInNewTabAction);
    addActionToMenu(action::EditCameraBookmarkAction, m_editBookmarkAction);
    addActionToMenu(action::ExportBookmarkAction, m_exportBookmarkAction);
    if (nx::vms::client::desktop::ini().enableCaseExport)
        addActionToMenu(action::ExportBookmarksAction, m_exportBookmarksAction);
    addActionToMenu(action::RemoveBookmarksAction, m_removeBookmarksAction);

    /* Connect action signal handlers: */
    connect(m_openInNewTabAction, &QAction::triggered, this,
        [this, params, window]
        {
            openInNewLayout(params, window);
        });

    connect(m_editBookmarkAction, &QAction::triggered, this,
        [this, params]
        {
            menu()->triggerIfPossible(action::EditCameraBookmarkAction, params);
        });

    connect(m_removeBookmarksAction, &QAction::triggered, this,
        [this, params]
        {
            const auto parentWidget = QPointer<QWidget>(m_owner);
            menu()->triggerIfPossible(action::RemoveBookmarksAction,
                action::Parameters(params).withArgument(Qn::ParentWidgetRole, parentWidget));
        });

    connect(m_exportBookmarkAction, &QAction::triggered, this,
        [this, params]
        {
            menu()->triggerIfPossible(action::ExportBookmarkAction, params);
        });

    connect(m_exportBookmarksAction,  &QAction::triggered, this,
        [this, params]
        {
            menu()->triggerIfPossible(action::ExportBookmarksAction, params);
        });

    /* Execute popup menu: */
    newMenu->exec(QCursor::pos());
    newMenu->deleteLater();

    /* Disconnect action signal handlers: */
    m_openInNewTabAction->disconnect(this);
    m_editBookmarkAction->disconnect(this);
    m_removeBookmarksAction->disconnect(this);
    m_exportBookmarksAction->disconnect(this);
    m_exportBookmarkAction->disconnect(this);
}
