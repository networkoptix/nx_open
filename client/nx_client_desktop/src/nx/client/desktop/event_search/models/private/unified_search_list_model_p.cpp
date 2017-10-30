#include "unified_search_list_model_p.h"

#include <chrono>

#include <api/server_rest_connection.h>
#include <api/helpers/event_log_request_data.h>
#include <camera/camera_bookmarks_manager.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/software_trigger_pixmaps.h>
#include <ui/workbench/workbench_access_controller.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/strings_helper.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

// Period of time displayed by default when a new camera is selected.
static constexpr auto kDefaultEventsPeriod = std::chrono::hours(24);

// In "live mode", every kUpdateTimerInterval events are fetched for the last kEventsUpdatePeriod.
static constexpr auto kEventsUpdatePeriod = std::chrono::seconds(30);
static constexpr auto kUpdateTimerInterval = kEventsUpdatePeriod - std::chrono::seconds(10);

static constexpr auto kEventsQueryTimeout = std::chrono::seconds(15);

} // namespace

UnifiedSearchListModel::Private::Private(UnifiedSearchListModel* q):
    base_type(),
    QnWorkbenchContextAware(q),
    q(q),
    m_updateTimer(new QTimer()),
    m_helper(new vms::event::StringsHelper(commonModule()))
{
    m_updateTimer->setInterval(std::chrono::milliseconds(kUpdateTimerInterval).count());
    connect(m_updateTimer.data(), &QTimer::timeout, this, &Private::periodicUpdate);
    watchBookmarkChanges();

    connect(q, &QAbstractItemModel::modelAboutToBeReset, this,
        [this]()
        {
            m_eventRequestHandles.clear();
            m_eventIds.clear();
        });
}

UnifiedSearchListModel::Private::~Private()
{
}

void UnifiedSearchListModel::Private::periodicUpdate()
{
    if (m_endTimeMs >= 0)
        return; //< If not live.

    NX_EXPECT(m_camera);
    if (!m_camera)
        return;

    // TODO: FIXME: Don't do update if finite time interval is set

    const auto endTimeMs = qnSyncTime->currentMSecsSinceEpoch();
    const auto startTimeMs = endTimeMs - std::chrono::milliseconds(kDefaultEventsPeriod).count();
    fetchEvents(startTimeMs, endTimeMs);
}

void UnifiedSearchListModel::Private::fetchEvents(qint64 startTimeMs, qint64 endTimeMs)
{
    const auto eventsReceived =
        [this, guard = QPointer<QObject>(this)]
            (bool success, rest::Handle handle, rest::EventLogData data)
            {
                if (!guard || !m_eventRequestHandles.contains(handle))
                    return;

                m_eventRequestHandles.remove(handle);

                if (!success)
                    return;

                for (const auto& event: data.data)
                    addCameraEvent(event);
            };

    QnEventLogRequestData request;
    request.cameras << m_camera;
    request.period = QnTimePeriod::fromInterval(startTimeMs, endTimeMs);

    const auto onlineServers = resourcePool()->getAllServers(Qn::Online);
    for (const auto& server: onlineServers)
    {
        const auto handle = server->restConnection()->getEvents(request, eventsReceived, thread());
        if (handle <= 0)
            continue;

        m_eventRequestHandles.insert(handle);

        const auto timeoutCallback =
            [this, handle, eventsReceived]
            {
                eventsReceived(false, handle, rest::EventLogData());
            };

        executeDelayedParented(timeoutCallback,
            std::chrono::milliseconds(kEventsQueryTimeout).count(),
            this);
    }
}

void UnifiedSearchListModel::Private::fetchBookmarks(qint64 startTimeMs, qint64 endTimeMs)
{
    if (!accessController()->hasGlobalPermission(Qn::GlobalViewBookmarksPermission))
        return;

    QnCameraBookmarkSearchFilter filter;
    filter.startTimeMs = startTimeMs;
    filter.endTimeMs = endTimeMs;

    qnCameraBookmarksManager->getBookmarksAsync({m_camera}, filter,
        [this](bool success, const QnCameraBookmarkList& bookmarks)
        {
            if (!success)
                return;

            for (const auto& bookmark: bookmarks)
                addOrUpdateBookmark(bookmark);
        });
}

void UnifiedSearchListModel::Private::fetchAnalytics(qint64 startTimeMs, qint64 endTimeMs)
{
    // TODO: FIXME: IMPLEMENTME: #vkutin
}

void UnifiedSearchListModel::Private::watchBookmarkChanges()
{
    // TODO: #vkutin Check whether qnCameraBookmarksManager won't emit these signals
    // if current user has no Qn::GlobalViewBookmarksPermission

    connect(qnCameraBookmarksManager, &QnCameraBookmarksManager::bookmarkAdded, this,
        [this](const QnCameraBookmark& bookmark) { addOrUpdateBookmark(bookmark); });

    connect(qnCameraBookmarksManager, &QnCameraBookmarksManager::bookmarkUpdated, this,
        [this](const QnCameraBookmark& bookmark) { addOrUpdateBookmark(bookmark); });

    connect(qnCameraBookmarksManager, &QnCameraBookmarksManager::bookmarkRemoved, this,
        [this](const QnUuid& bookmarkId) { q->removeEvent(bookmarkId); });
}

void UnifiedSearchListModel::Private::addOrUpdateBookmark(const QnCameraBookmark& bookmark)
{
    if (bookmark.startTimeMs < m_startTimeMs
        || m_endTimeMs >= 0 && bookmark.startTimeMs > m_endTimeMs)
    {
        // Skip bookmarks outside of time range.
        return;
    }

    EventData data;
    data.id = bookmark.guid;
    data.title = bookmark.name;
    data.description = bookmark.description;
    data.timestamp = bookmark.startTimeMs;
    data.icon = qnSkin->pixmap(lit("buttons/bookmark.png"));
    data.helpId = Qn::Bookmarks_Usage_Help;
    //data.titleColor;
    //data.actionId =
    //data.actionParameters =

    if (!q->updateEvent(data))
        q->addEvent(data);
}

void UnifiedSearchListModel::Private::addCameraEvent(const nx::vms::event::ActionData& event)
{
    const auto eventTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::microseconds(event.eventParams.eventTimestampUsec)).count();

    if (eventTimeMs < m_startTimeMs || (m_endTimeMs >= 0 && eventTimeMs > m_endTimeMs))
        return; // Skip events outside of time range.

    const auto eventId = qMakePair(event.businessRuleId, event.eventParams.eventTimestampUsec);
    if (m_eventIds.contains(eventId))
        return; //< Event already added.

    const auto id = QnUuid::createUuid();
    m_eventIds[eventId] = id;

    EventData data;
    data.id = id;
    data.title = m_helper->eventName(event.eventParams.eventType);
    data.description = m_helper->eventDetails(event.eventParams).join(lit("<br>"));
    data.timestamp = eventTimeMs;
    data.icon = eventPixmap(event.eventParams);
    data.titleColor = eventColor(event.eventParams.eventType);
    //data.helpId = Qn::Bookmarks_Usage_Help;
    //data.actionId =
    //data.actionParameters =

    q->addEvent(data);
}

QnVirtualCameraResourcePtr UnifiedSearchListModel::Private::camera() const
{
    return m_camera;
}

void UnifiedSearchListModel::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_camera == camera)
        return;

    m_camera = camera;

    q->clear(); //< Will call d->clear().

    m_startTimeMs = m_endTimeMs = 0;
    m_updateTimer->stop();

    if (!m_camera)
        return;

    // TODO: #vkutin #gdm #common Think of some means to prevent overwhelming server
    // with queries if current camera is quickly switched many times.

    const auto endTimeMs = qnSyncTime->currentMSecsSinceEpoch();
    m_startTimeMs = endTimeMs - std::chrono::milliseconds(kDefaultEventsPeriod).count();
    m_endTimeMs = -1; //< Reset to live.

    fetchEvents(m_startTimeMs, endTimeMs);
    fetchBookmarks(m_startTimeMs, endTimeMs);
    fetchAnalytics(m_startTimeMs, endTimeMs);

    m_updateTimer->start();
}

QPixmap UnifiedSearchListModel::Private::eventPixmap(const nx::vms::event::EventParameters& event)
{
    switch (event.eventType)
    {
        case nx::vms::event::EventType::cameraMotionEvent:
            return QPixmap();

        case nx::vms::event::EventType::cameraInputEvent:
            return QPixmap();

        case nx::vms::event::EventType::cameraDisconnectEvent:
            return QPixmap();

        case nx::vms::event::EventType::storageFailureEvent:
            return QPixmap();

        case nx::vms::event::EventType::networkIssueEvent:
            return QPixmap();

        case nx::vms::event::EventType::cameraIpConflictEvent:
            return QPixmap();

        case nx::vms::event::EventType::serverFailureEvent:
            return QPixmap();

        case nx::vms::event::EventType::serverConflictEvent:
            return QPixmap();

        case nx::vms::event::EventType::serverStartEvent:
            return QPixmap();

        case nx::vms::event::EventType::licenseIssueEvent:
            return QPixmap();

        case nx::vms::event::EventType::backupFinishedEvent:
            return QPixmap();

        case nx::vms::event::EventType::softwareTriggerEvent:
            return QnSoftwareTriggerPixmaps::pixmapByName(event.description);

        case nx::vms::event::EventType::analyticsSdkEvent:
            return QPixmap();

        default:
            return QPixmap();
    }
}

QColor UnifiedSearchListModel::Private::eventColor(nx::vms::event::EventType eventType)
{
    switch (eventType)
    {
        case nx::vms::event::EventType::cameraDisconnectEvent:
        case nx::vms::event::EventType::storageFailureEvent:
        case nx::vms::event::EventType::serverFailureEvent:
            return qnGlobals->errorTextColor();

        case nx::vms::event::EventType::networkIssueEvent:
        case nx::vms::event::EventType::cameraIpConflictEvent:
        case nx::vms::event::EventType::serverConflictEvent:
            return qnGlobals->warningTextColor();

        case nx::vms::event::EventType::backupFinishedEvent:
            return qnGlobals->successTextColor();

        //case nx::vms::event::EventType::licenseIssueEvent: //< TODO: normal or warning?
        default:
            return QColor();
    }
}

} // namespace
} // namespace client
} // namespace nx
