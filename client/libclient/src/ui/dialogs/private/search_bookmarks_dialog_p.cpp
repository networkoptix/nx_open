#include "search_bookmarks_dialog_p.h"

#include "ui_search_bookmarks_dialog.h"

#include <client/client_settings.h>

#include <common/common_module.h>

#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>

#include <recording/time_period.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/common/read_only.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/models/search_bookmarks_model.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

#include <utils/common/synctime.h>
#include <utils/common/scoped_value_rollback.h>

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

    , m_openInNewTabAction      (new QAction(action(QnActions::OpenInNewLayoutAction)->text(), this))
    , m_editBookmarkAction      (new QAction(action(QnActions::EditCameraBookmarkAction)->text(), this))
    , m_exportBookmarkAction    (new QAction(tr("Export Bookmark..."), this))
    , m_removeBookmarksAction   (new QAction(action(QnActions::RemoveBookmarksAction)->text(), this))
    , m_updatingNow(false)
{
    m_ui->setupUi(m_owner);
    m_ui->refreshButton->setIcon(qnSkin->icon("buttons/refresh.png"));
    m_ui->clearFilterButton->setIcon(qnSkin->icon("buttons/clear.png"));

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

    connect(m_ui->filterLineEdit, &QnSearchLineEdit::enterKeyPressed, this, updateFilterText);
    connect(m_ui->filterLineEdit, &QnSearchLineEdit::textChanged, this, updateFilterText);

    connect(m_ui->filterLineEdit, &QnSearchLineEdit::escKeyPressed, this, [this]()
    {
        m_ui->filterLineEdit->lineEdit()->setText(QString());
        m_model->setFilterText(QString());
        applyModelChanges();
    });

    connect(m_ui->dateRangeWidget, &QnDateRangeWidget::rangeChanged, this,
        [this](qint64 startTimeMs, qint64 endTimeMs)
        {
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
        QnActionParameters params;
        QnTimePeriod window;
        if (!fillActionParameters(params, window))
            return;

        if (!menu()->canTrigger(QnActions::OpenInNewLayoutAction, params))
            return;

        openInNewLayout(params, window);
    });

    /* Context menu actions are connected in customContextMenuRequested() */

    setParameters(filterText, utcStartTimeMs, utcFinishTimeMs);

    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnSearchBookmarksDialogPrivate::reset);


    m_ui->filterLineEdit->lineEdit()->setPlaceholderText(tr("Search bookmarks by name, tag or description"));
}

QnSearchBookmarksDialogPrivate::~QnSearchBookmarksDialogPrivate()
{
}

void QnSearchBookmarksDialogPrivate::applyModelChanges()
{
    if (!m_updatingNow)
        m_model->applyFilter();
}

void QnSearchBookmarksDialogPrivate::reset()
{
    {
        QN_SCOPED_VALUE_ROLLBACK(&m_updatingNow, true);
        resetToAllAvailableCameras();
        m_ui->filterLineEdit->clear();
        m_ui->dateRangeWidget->reset();
    }

    applyModelChanges();
}

void QnSearchBookmarksDialogPrivate::setParameters(const QString &filterText
    , qint64 utcStartTimeMs
    , qint64 utcFinishTimeMs)
{
    {
        QN_SCOPED_VALUE_ROLLBACK(&m_updatingNow, true);

        resetToAllAvailableCameras();

        m_ui->filterLineEdit->lineEdit()->setText(filterText);
        m_ui->dateRangeWidget->setRange(utcStartTimeMs, utcFinishTimeMs);

        m_model->setFilterText(filterText);
        /* Start/end values are rounded in the widget to 1-day granularity. */
        m_model->setRange(m_ui->dateRangeWidget->startTimeMs(), m_ui->dateRangeWidget->endTimeMs());
    }

    applyModelChanges();
}

bool QnSearchBookmarksDialogPrivate::fillActionParameters(QnActionParameters &params, QnTimePeriod &window)
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


    params = QnActionParameters();

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

void QnSearchBookmarksDialogPrivate::openInNewLayout(const QnActionParameters &params, const QnTimePeriod &window)
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

    menu()->trigger(QnActions::OpenInNewLayoutAction, params);

    static const qint64 kMinOffset = 30 * 1000;
    const auto offset = std::max(window.durationMs, kMinOffset);

    const QnTimePeriod extendedWindow(window.startTimeMs - offset
        , window.durationMs + offset * 2);
    setFirstLayoutItemPeriod(extendedWindow, Qn::ItemSliderWindowRole);

    menu()->trigger(QnActions::BookmarksModeAction);
}

void QnSearchBookmarksDialogPrivate::updateHeadersWidth()
{
    enum
    {
        kNamePart = 8
        , kStartTimePart = 3
        , kLengthPart = 3
        , kTagsPart = 4
        , kCameraPart = 3
        , kPartsTotalNumber = kNamePart + kStartTimePart + kLengthPart + kTagsPart + kCameraPart
    };

    QHeaderView * const header = m_ui->gridBookmarks->horizontalHeader();
    const int totalWidth = header->width();
    header->resizeSection(QnSearchBookmarksModel::kName, totalWidth * kNamePart / kPartsTotalNumber);
    header->resizeSection(QnSearchBookmarksModel::kStartTime, totalWidth * kStartTimePart / kPartsTotalNumber);
    header->resizeSection(QnSearchBookmarksModel::kLength, totalWidth * kLengthPart / kPartsTotalNumber);
    header->resizeSection(QnSearchBookmarksModel::kTags, totalWidth * kTagsPart / kPartsTotalNumber);
    header->resizeSection(QnSearchBookmarksModel::kCamera, totalWidth * kCameraPart / kPartsTotalNumber);
}

void QnSearchBookmarksDialogPrivate::refresh()
{
    {
        QN_SCOPED_VALUE_ROLLBACK(&m_updatingNow, true);
        m_model->setFilterText(m_ui->filterLineEdit->lineEdit()->text());
        m_model->setRange(m_ui->dateRangeWidget->startTimeMs(), m_ui->dateRangeWidget->endTimeMs());
    }
    m_model->applyFilter();
}


QnVirtualCameraResourceList QnSearchBookmarksDialogPrivate::availableCameras() const
{
    return qnResPool->getAllCameras(QnResourcePtr(), true).filtered(
        [this](const QnVirtualCameraResourcePtr& camera)
        {
            return accessController()->hasPermissions(camera, Qn::ReadPermission);
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
        m_ui->cameraButton->selectFew(cameras);
}

void QnSearchBookmarksDialogPrivate::chooseCamera()
{
    QnResourceSelectionDialog dialog(QnResourceSelectionDialog::Filter::cameras, m_owner);
    QSet<QnUuid> ids;
    if (!m_allCamerasChoosen)
    {
        for (auto camera: m_model->cameras())
            ids << camera->getId();
    }
    dialog.setSelectedResources(ids);

    if (dialog.exec() == QDialog::Accepted)
    {
        auto selectedCameras = qnResPool->getResources<QnVirtualCameraResource>(
            dialog.selectedResources());
        setCameras(selectedCameras);
        m_model->applyFilter();
    }
}

bool QnSearchBookmarksDialogPrivate::currentUserHasAllCameras()
{
    return accessController()->hasGlobalPermission(Qn::GlobalAccessAllMediaPermission);
}

void QnSearchBookmarksDialogPrivate::customContextMenuRequested()
{
    /* Fill action parameters: */
    QnActionParameters params;
    QnTimePeriod window;
    if (!fillActionParameters(params, window))
        return;

    /* Create menu object: */
    auto newMenu = new QMenu();

    /* Add suitable actions: */
    const auto addActionToMenu = [this, newMenu, params](QnActions::IDType id, QAction *action)
    {
        if (menu()->canTrigger(id, params))
            newMenu->addAction(action);
    };

    addActionToMenu(QnActions::OpenInNewLayoutAction, m_openInNewTabAction);
    addActionToMenu(QnActions::EditCameraBookmarkAction, m_editBookmarkAction);
    addActionToMenu(QnActions::ExportTimeSelectionAction, m_exportBookmarkAction);
    addActionToMenu(QnActions::RemoveBookmarksAction, m_removeBookmarksAction);

    /* Connect action signal handlers: */
    connect(m_openInNewTabAction, &QAction::triggered, this, [this, params, window]
    {
        openInNewLayout(params, window);
    });

    connect(m_editBookmarkAction, &QAction::triggered, this, [this, params]
    {
        menu()->triggerIfPossible(QnActions::EditCameraBookmarkAction, params);
    });

    connect(m_removeBookmarksAction, &QAction::triggered, this, [this, params]
    {
        menu()->triggerIfPossible(QnActions::RemoveBookmarksAction, params);
    });

    connect(m_exportBookmarkAction, &QAction::triggered, this, [this, params]
    {
        menu()->triggerIfPossible(QnActions::ExportTimeSelectionAction, params);
    });

    /* Execute popup menu: */
    newMenu->exec(QCursor::pos());
    newMenu->deleteLater();

    /* Disconnect action signal handlers: */
    m_openInNewTabAction->disconnect(this);
    m_editBookmarkAction->disconnect(this);
    m_removeBookmarksAction->disconnect(this);
    m_exportBookmarkAction->disconnect(this);
}
