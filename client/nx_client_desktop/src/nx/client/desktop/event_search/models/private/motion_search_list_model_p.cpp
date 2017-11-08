#include "motion_search_list_model_p.h"

#include <limits>

#include <core/resource/camera_resource.h>
#include <camera/loaders/caching_camera_data_loader.h>
#include <ui/help/business_help.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

#include <nx/client/core/utils/human_readable.h>
#include <nx/vms/event/event_fwd.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr int kFetchBatchSize = 20;

} // namespace

MotionSearchListModel::Private::Private(MotionSearchListModel* q):
    QObject(),
    q(q)
{
}

MotionSearchListModel::Private::~Private()
{
}

QnVirtualCameraResourcePtr MotionSearchListModel::Private::camera() const
{
    return m_camera;
}

void MotionSearchListModel::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (camera == m_camera)
        return;

    if (m_loader)
        m_loader->disconnect(this);

    m_camera = camera;

    m_loader = m_camera
        ? q->navigator()->cameraDataManager()->loader(m_camera, false)
        : QnCachingCameraDataLoaderPtr();

    if (!m_loader)
        return;

    connect(m_loader.data(), &QnCachingCameraDataLoader::periodsChanged, this,
        [this](Qn::TimePeriodContent type, qint64 startTimeMs)
        {
            if (type == Qn::MotionContent)
                periodsChanged(startTimeMs);
        });
}

void MotionSearchListModel::Private::periodsChanged(qint64 startTimeMs)
{
    if (startTimeMs == 0 || q->rowCount() == 0)
    {
        q->clear();
        q->fetchMore();
        return;
    }

    const auto periods = m_loader->periods(Qn::MotionContent);
    const auto rangeBegin = std::upper_bound(periods.cbegin(), periods.cend(), lastTimeMs(),
        [](qint64 left, const QnTimePeriod& right) { return left < right.startTimeMs; });

    const auto count = std::distance(rangeBegin, periods.end());
    static constexpr int kIncrementalUpdateLimit = 1000;

    NX_ASSERT(count <= kIncrementalUpdateLimit);
    if (count > kIncrementalUpdateLimit)
    {
        q->clear();
        q->fetchMore();
        return;
    }

    for (auto iter = rangeBegin; iter != periods.end(); ++iter)
        addPeriod(*iter, Position::front);
}

void MotionSearchListModel::Private::addPeriod(const QnTimePeriod& period, Position where)
{
    const auto timeWatcher = q->context()->instance<QnWorkbenchServerTimeWatcher>();
    const auto start = timeWatcher->displayTime(period.startTimeMs);

    EventData eventData;
    eventData.id = QnUuid::createUuid();
    eventData.title = tr("Motion on camera");
    eventData.description = lit("%1: %2<br>%3: %4")
        .arg(tr("Start"))
        .arg(start.toString(Qt::RFC2822Date))
        .arg(tr("Duration"))
        .arg(core::HumanReadable::timeSpan(std::chrono::milliseconds(period.durationMs)));

    eventData.helpId = QnBusiness::eventHelpId(vms::event::cameraMotionEvent);
    eventData.timestampMs = period.startTimeMs;
    eventData.previewCamera = m_camera;
    //eventData.icon = ;

    q->addEvent(eventData, where);
}

bool MotionSearchListModel::Private::canFetchMore() const
{
    const auto periods = m_loader->periods(Qn::MotionContent);
    return !periods.empty() && firstTimeMs() > periods[0].startTimeMs;
}

void MotionSearchListModel::Private::fetchMore()
{
    const auto periods = m_loader->periods(Qn::MotionContent);
    const auto rangeBegin = std::upper_bound(periods.crbegin(), periods.crend(), firstTimeMs(),
        [](qint64 left, const QnTimePeriod& right) { return left > right.startTimeMs; });

    const auto remaining = std::distance(rangeBegin, periods.crend());
    const auto rangeEnd = rangeBegin + qMin(ptrdiff_t(kFetchBatchSize), remaining);

    for (auto iter = rangeBegin; iter != rangeEnd; ++iter)
        addPeriod(*iter, Position::back);

    q->finishFetch();
}

qint64 MotionSearchListModel::Private::firstTimeMs() const
{
    return q->rowCount() > 0
        ? q->getEvent(q->index(q->rowCount() - 1)).timestampMs
        : std::numeric_limits<qint64>::max();
}

qint64 MotionSearchListModel::Private::lastTimeMs() const
{
    return q->rowCount() > 0
        ? q->getEvent(q->index(0)).timestampMs
        : 0;
}

} // namespace
} // namespace client
} // namespace nx
