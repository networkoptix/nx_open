#include "analytics_search_list_model_p.h"

#include <chrono>

#include <QtGui/QPalette>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <ui/style/helper.h>
#include <ui/workbench/workbench_access_controller.h>
#include <utils/common/synctime.h>

namespace nx {
namespace client {
namespace desktop {

using namespace analytics::storage;

namespace {

static constexpr int kFetchBatchSize = 25;

static qint64 startTimeUs(const DetectedObject& object)
{
    NX_ASSERT(!object.track.empty());
    return object.track.empty() ? 0 : object.track.front().timestampUsec;
}

static const auto upperBoundPredicate =
    [](qint64 left, const DetectedObject& right)
    {
        return left > startTimeUs(right);
    };

} // namespace

AnalyticsSearchListModel::Private::Private(AnalyticsSearchListModel* q):
    base_type(),
    q(q)
{
}

AnalyticsSearchListModel::Private::~Private()
{
}

QnVirtualCameraResourcePtr AnalyticsSearchListModel::Private::camera() const
{
    return m_camera;
}

void AnalyticsSearchListModel::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_camera == camera)
        return;

    m_camera = camera;
    clear();

    // TODO: #vkutin Subscribe to analytics metadata in RTSP stream. Process it as required.
}

int AnalyticsSearchListModel::Private::count() const
{
    return int(m_data.size());
}

const analytics::storage::DetectedObject& AnalyticsSearchListModel::Private::object(int index) const
{
    return m_data[index];
}

void AnalyticsSearchListModel::Private::clear()
{
    m_prefetch.clear();
    m_fetchedAll = false;

    if (!m_data.empty())
    {
        ScopedReset reset(q);
        m_data.clear();
    }

    m_earliestTimeMs = std::numeric_limits<qint64>::max();
}

bool AnalyticsSearchListModel::Private::canFetchMore() const
{
    return !m_fetchedAll && m_camera && m_prefetch.empty()
        && q->accessController()->hasGlobalPermission(Qn::GlobalViewLogsPermission);
}

bool AnalyticsSearchListModel::Private::prefetch(PrefetchCompletionHandler completionHandler)
{
    if (!canFetchMore() || !completionHandler)
        return false;

    const auto dataReceived =
        [this, completionHandler, guard = QPointer<QObject>(this)]
            (bool success, rest::Handle /*handle*/, analytics::storage::LookupResult&& data)
        {
            if (!guard)
                return;

            NX_ASSERT(m_prefetch.empty());
            m_prefetch = success ? std::move(data) : analytics::storage::LookupResult();
            m_success = success;

            NX_ASSERT(m_prefetch.empty() || !m_prefetch.front().track.empty());
            completionHandler(m_prefetch.size() >= kFetchBatchSize
                ? startTimeMs(m_prefetch.front()) + 1 /*discard last ms*/
                : 0);
        };

    const auto server = q->commonModule()->currentServer();
    NX_ASSERT(server && server->restConnection());
    if (!server || !server->restConnection())
        return false;

    analytics::storage::Filter request;
    request.deviceId = m_camera->getId();
    request.timePeriod = QnTimePeriod::fromInterval(0, m_earliestTimeMs - 1);
    request.sortOrder = Qt::DescendingOrder;
    request.maxObjectsToSelect = kFetchBatchSize;

    return server->restConnection()->lookupDetectedObjects(request, dataReceived, thread());
}

void AnalyticsSearchListModel::Private::commitPrefetch(qint64 latestStartTimeMs)
{
    if (!m_success)
        return;

    const auto latestTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::milliseconds(latestStartTimeMs)).count();

    const auto end = std::upper_bound(m_prefetch.begin(), m_prefetch.end(),
        latestTimeUs, upperBoundPredicate);

    const auto first = this->count();
    const auto count = std::distance(m_prefetch.begin(), end);

    if (count > 0)
    {
        ScopedInsertRows insertRows(q, QModelIndex(), first, first + count - 1);
        for (auto iter = m_prefetch.begin(); iter != end; ++iter)
            m_data.push_back(std::move(*iter));
    }

    m_fetchedAll = count == m_prefetch.size() && m_prefetch.size() < kFetchBatchSize;
    m_earliestTimeMs = latestStartTimeMs;
    m_prefetch.clear();
}

QString AnalyticsSearchListModel::Private::description(
    const analytics::storage::DetectedObject& object)
{
    if (object.attributes.empty())
        return QString();

    static const auto kCss = QString::fromLatin1(R"(
            <style type = 'text/css'>
                th { color: %1; font-weight: normal; }
            </style>)");

    static const auto kTableTemplate = lit("<table cellpadding='0' cellspacing='0'>%1</table>");
    static const auto kRowTemplate = lit("<tr><th>%1</th>")
        + lit("<td width='%1'/>").arg(style::Metrics::kStandardPadding) //< Spacing.
        + lit("<td>%2</td></tr>");

    QString rows;
    for (const auto& attribute : object.attributes)
        rows += kRowTemplate.arg(attribute.name, attribute.value);

    const auto color = QPalette().color(QPalette::WindowText);
    return kCss.arg(color.name()) + kTableTemplate.arg(rows);
}

qint64 AnalyticsSearchListModel::Private::startTimeMs(
    const analytics::storage::DetectedObject& object)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::microseconds(startTimeUs(object))).count();
}

} // namespace
} // namespace client
} // namespace nx
