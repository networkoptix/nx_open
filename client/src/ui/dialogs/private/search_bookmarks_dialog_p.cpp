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
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>
#include <ui/workbench/watchers/current_user_available_cameras_watcher.h>

#include <utils/common/synctime.h>
#include <utils/common/scoped_value_rollback.h>

namespace
{
    enum
    {
        kMillisecondsInSeconds = 1000
        , kStartDayOffsetMs = 0
        , kMillisecondsInDay = 60 * 60 * 24 * kMillisecondsInSeconds
    };

    qint64 getStartOfTheDayMs(qint64 timeMs)
    {
        return timeMs - (timeMs % kMillisecondsInDay);
    }

    qint64 getEndOfTheDayMs(qint64 timeMs)
    {
        return getStartOfTheDayMs(timeMs) + kMillisecondsInDay;
    }

    qint64 getTimeOnServerMs(QnWorkbenchContext *context
        , const QDate &clientDate
        , qint64 dayEndOffset = kStartDayOffsetMs)
    {
        if (qnSettings->timeMode() == Qn::ClientTimeMode)
        {
            // QDateTime is created from date, thus it always started
            // from the start of the day in current timezone
            return QDateTime(clientDate).toMSecsSinceEpoch() + dayEndOffset;
        }

        const auto timeWatcher = context->instance<QnWorkbenchServerTimeWatcher>();
        const auto server = qnCommon->currentServer();
        const auto serverUtcOffsetSecs = timeWatcher->utcOffset(server) / kMillisecondsInSeconds;
        const QDateTime serverTime(clientDate, QTime(0, 0), Qt::OffsetFromUTC, serverUtcOffsetSecs);
        return serverTime.toMSecsSinceEpoch() + dayEndOffset;
    }

    QDate extractDisplayDate(QnWorkbenchContext *context
        , qint64 timestamp)
    {
        return context->instance<QnWorkbenchServerTimeWatcher>()->displayTime(timestamp).date();
    };
}


///

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
    , m_exportBookmarkAction    (new QAction(tr("Export bookmark..."), this))
    , m_removeBookmarksAction   (new QAction(action(QnActions::RemoveBookmarksAction)->text(), this))
    , m_updatingNow(false)
{
    m_ui->setupUi(m_owner);
    m_ui->gridBookmarks->setModel(m_model);

    QnBookmarkSortOrder sortOrder = QnSearchBookmarksModel::defaultSortOrder();
    int column = QnSearchBookmarksModel::sortFieldToColumn(sortOrder.column);
    m_ui->gridBookmarks->horizontalHeader()->setSortIndicator(column, sortOrder.order);

    const auto updateFilterText = [this]()
    {
        m_model->setFilterText(m_ui->filterLineEdit->lineEdit()->text());
        applyModelChanges();
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

    connect(m_ui->dateEditFrom, &QDateEdit::userDateChanged, this, [this](const QDate &date)
    {
        m_model->setRange(getTimeOnServerMs(context(), date)
            , getTimeOnServerMs(context(), m_ui->dateEditTo->date(), kMillisecondsInDay));
        applyModelChanges();
    });
    connect(m_ui->dateEditTo, &QDateEdit::userDateChanged, this, [this](const QDate &date)
    {
        m_model->setRange(getTimeOnServerMs(context(), m_ui->dateEditFrom->date())
            , getTimeOnServerMs(context(), date, kMillisecondsInDay));
        applyModelChanges();
    });

    connect(m_ui->refreshButton, &QPushButton::clicked, this, &QnSearchBookmarksDialogPrivate::refresh);
    connect(m_ui->cameraButton, &QPushButton::clicked, this, &QnSearchBookmarksDialogPrivate::chooseCamera);

    connect(m_ui->gridBookmarks, &QTableView::customContextMenuRequested
        , this, &QnSearchBookmarksDialogPrivate::customContextMenuRequested);

    connect(m_ui->gridBookmarks, &QTableView::doubleClicked, this
        , [this](const QModelIndex & /* index */)
    {
        openInNewLayoutHandler();
    });

    connect(m_openInNewTabAction, &QAction::triggered, this, &QnSearchBookmarksDialogPrivate::openInNewLayoutHandler);
    connect(m_exportBookmarkAction, &QAction::triggered, this, &QnSearchBookmarksDialogPrivate::exportBookmarkHandler);
    connect(m_removeBookmarksAction, &QAction::triggered, this, [this]
    {
        QnActionParameters params;
        QnTimePeriod window;    //TODO: remove this variable requirement
        if (!fillActionParameters(params, window))
            return;

        menu()->triggerIfPossible(QnActions::RemoveBookmarksAction, params);
    });

    setParameters(filterText, utcStartTimeMs, utcFinishTimeMs);

    const auto camerasWatcher = context()->instance<QnCurrentUserAvailableCamerasWatcher>();
    connect(camerasWatcher, &QnCurrentUserAvailableCamerasWatcher::userChanged, this, [this]()
    {
        resetToAllAvailableCameras();
        applyModelChanges();
    });

    connect(camerasWatcher, &QnCurrentUserAvailableCamerasWatcher::availableCamerasChanged, this, [this]()
    {
        if (m_allCamerasChoosen)
            resetToAllAvailableCameras();
    });

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

void QnSearchBookmarksDialogPrivate::setParameters(const QString &filterText
    , qint64 utcStartTimeMs
    , qint64 utcFinishTimeMs)
{
    /* Working with the 1-day precision */
    qint64 startOfTheDayMs = getStartOfTheDayMs(utcStartTimeMs);
    qint64 endOfTheDayMs = getEndOfTheDayMs(utcFinishTimeMs);

    {
        QN_SCOPED_VALUE_ROLLBACK(&m_updatingNow, true);

        resetToAllAvailableCameras();

        m_ui->filterLineEdit->lineEdit()->setText(filterText);
        m_ui->dateEditFrom->setDate(extractDisplayDate(context(), startOfTheDayMs));
        m_ui->dateEditTo->setDate(extractDisplayDate(context(), endOfTheDayMs));

        m_model->setFilterText(filterText);
        m_model->setRange(startOfTheDayMs, endOfTheDayMs);
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

        if (!availableCameraByUniqueId(bookmark.cameraId))
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

    auto camera = availableCameraByUniqueId(currentBookmark.cameraId);
    if (camera)
        params.setResources(QnResourceList() << camera);

    window = QnTimePeriod(currentBookmark.startTimeMs, currentBookmark.durationMs);
    params.setArgument(Qn::TimePeriodRole, window);
    params.setArgument(Qn::ItemTimeRole, window.startTimeMs);

    return true;
}

void QnSearchBookmarksDialogPrivate::openInNewLayoutHandler()
{
    QnActionParameters params;
    QnTimePeriod window;
    if (!fillActionParameters(params, window))
        return;

    if (!menu()->canTrigger(QnActions::OpenInNewLayoutAction, params))
        return;

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

void QnSearchBookmarksDialogPrivate::exportBookmarkHandler()
{
    QnActionParameters params;
    QnTimePeriod window;
    if (!fillActionParameters(params, window))
        return;

    menu()->triggerIfPossible(QnActions::ExportTimeSelectionAction, params);
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
    m_model->setFilterText(m_ui->filterLineEdit->lineEdit()->text());
    m_model->setRange(getTimeOnServerMs(context(), m_ui->dateEditFrom->date())
        , getTimeOnServerMs(context(), m_ui->dateEditTo->date(), kMillisecondsInDay));

    m_model->applyFilter();
}


QnVirtualCameraResourceList QnSearchBookmarksDialogPrivate::availableCameras() const
{
    const auto camerasWatcher = context()->instance<QnCurrentUserAvailableCamerasWatcher>();
    return camerasWatcher->availableCameras();
}

QnVirtualCameraResourcePtr QnSearchBookmarksDialogPrivate::availableCameraByUniqueId(const QString &uniqueId) const
{
    const auto cameras = availableCameras();

    const auto it = std::find_if(cameras.begin(), cameras.end()
        , [this, uniqueId](const QnVirtualCameraResourcePtr &camera)
    {
        return (camera->getUniqueId() == uniqueId);
    });

    return (it == cameras.end() ? QnVirtualCameraResourcePtr() : *it);
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

    const bool isAdmin = currentUserHasAdminPrivileges();
    setReadOnly(m_ui->cameraButton, !isAdmin);

    if (cameras.empty())
    {
        static const auto kAnyDevicesStringsSet = QnCameraDeviceStringSet(
            tr("<Any Device>"), tr("<Any Camera>"), tr("<Any I/O Module>"));
        static const auto kAnyMyDevicesStringsSet = QnCameraDeviceStringSet(
            tr("<All My Devices>"), tr("<All My Cameras>"), tr("<All My I/O Modules>"));

        const auto &devicesStringsSet = (isAdmin
            ? kAnyDevicesStringsSet : kAnyMyDevicesStringsSet);

        m_ui->cameraButton->setText(QnDeviceDependentStrings::getNameFromSet(
            devicesStringsSet, correctCameras));
        return;
    }

    const auto devicesStringSet = QnCameraDeviceStringSet(
          tr("<%n device(s)>",      "", cameras.size())
        , tr("<%n camera(s)>",      "", cameras.size())
        , tr("<%n I/O module(s)>",  "", cameras.size()));

    const auto caption = QnDeviceDependentStrings::getNameFromSet(
        devicesStringSet, cameras);

    m_ui->cameraButton->setText(caption);
}

void QnSearchBookmarksDialogPrivate::chooseCamera()
{
    /// Do not allow user without administrator privileges to choose cameras
    if (!currentUserHasAdminPrivileges())
        return;

    QnResourceSelectionDialog dialog(m_owner);
    dialog.setSelectedResources(m_allCamerasChoosen
        ? QnVirtualCameraResourceList() : m_model->cameras());

    if (dialog.exec() == QDialog::Accepted)
    {
        setCameras(dialog.selectedResources().filtered<QnVirtualCameraResource>());
        m_model->applyFilter();
    }
}

bool QnSearchBookmarksDialogPrivate::currentUserHasAdminPrivileges()
{
    return accessController()->hasGlobalPermissions(Qn::GlobalAdminPermissions);
}

QMenu *QnSearchBookmarksDialogPrivate::createContextMenu(const QnActionParameters &params)
{
    auto result = new QMenu();

    const auto addActionToMenu = [this, result, params] (QnActions::IDType id, QAction *action)
    {
        if (menu()->canTrigger(id, params))
            result->addAction(action);
    };

    addActionToMenu(QnActions::OpenInNewLayoutAction, m_openInNewTabAction);
    addActionToMenu(QnActions::ExportTimeSelectionAction, m_exportBookmarkAction);
    addActionToMenu(QnActions::RemoveBookmarksAction, m_removeBookmarksAction);
    return result;
}

void QnSearchBookmarksDialogPrivate::customContextMenuRequested()
{
    QnActionParameters params;
    QnTimePeriod window;
    if (!fillActionParameters(params, window))
        return;

    if (auto newMenu = createContextMenu(params))
    {
        newMenu->exec(QCursor::pos());
        newMenu->deleteLater();
    }
}
