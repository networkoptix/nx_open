#include "motion_search_list_model_p.h"

#include <core/resource/camera_resource.h>
#include <camera/loaders/caching_camera_data_loader.h>
#include <ui/workbench/workbench_navigator.h>

#include <nx/vms/event/event_fwd.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr int kFetchBatchSize = 25;

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

    reset();

    if (!m_loader)
        return;

    const auto updatePeriods =
        [this](Qn::TimePeriodContent type, qint64 startTimeMs)
        {
            if (type == Qn::MotionContent)
                updateMotionPeriods(startTimeMs);
        };

    connect(m_loader.data(), &QnCachingCameraDataLoader::periodsChanged,
        this, updatePeriods, Qt::DirectConnection);
}

int MotionSearchListModel::Private::count() const
{
    return int(m_data.size());
}

const QnTimePeriod& MotionSearchListModel::Private::period(int index) const
{
    return *(m_data.crbegin() + index); //< Reverse!
}

void MotionSearchListModel::Private::reset()
{
    ScopedReset reset(q);
    m_data.clear();

    if (!m_loader)
        return;

    const auto& periods = m_loader->periods(Qn::MotionContent);
    m_data.insert(m_data.end(), periods.cbegin() + qMax(periods.size() - kFetchBatchSize, 0),
        periods.cend());
}

void MotionSearchListModel::Private::updateMotionPeriods(qint64 startTimeMs)
{
    const auto periods = m_loader->periods(Qn::MotionContent);

    if (startTimeMs == 0 || m_data.empty() || periods.empty())
    {
        reset();
        return;
    }

    const auto pred =
        [](const QnTimePeriod& left, qint64 right) { return left.startTimeMs < right; };

    auto removeBegin = std::lower_bound(m_data.begin(), m_data.end(), startTimeMs, pred);
    auto newChunksBegin = std::lower_bound(periods.cbegin(), periods.cend(), startTimeMs, pred);

    // Skip and keep chunks that weren't changed.
    while (removeBegin != m_data.end() && newChunksBegin != periods.cend())
    {
        if (!(*removeBegin == *newChunksBegin))
            break;

        ++removeBegin;
        ++newChunksBegin;
    }

    const auto removeCount = std::distance(removeBegin, m_data.end());
    if (removeCount > 0)
    {
        ScopedRemoveRows removeRows(q,  0, removeCount - 1);
        m_data.resize(m_data.size() - removeCount);
    }

    const auto newChunksCount = std::distance(newChunksBegin, periods.end());
    if (newChunksCount > 0)
    {
        static constexpr int kIncrementalUpdateLimit = 1000;
        if (newChunksCount > kIncrementalUpdateLimit)
        {
            reset();
            return;
        }

        ScopedInsertRows insertRows(q,  0, newChunksCount - 1);
        for (auto chunk = newChunksBegin; chunk != periods.cend(); ++chunk)
            m_data.push_back(*chunk);
    }
}

bool MotionSearchListModel::Private::canFetchMore() const
{
    return m_loader && count() < m_loader->periods(Qn::MotionContent).size();
}

void MotionSearchListModel::Private::fetchMore()
{
    if (!m_loader)
        return;

    const auto periods = m_loader->periods(Qn::MotionContent);
    const auto oldCount = count();

    const auto remaining = periods.size() - oldCount;
    if (remaining == 0)
        return;

    const auto delta = qMin(remaining, kFetchBatchSize);
    const auto newCount = oldCount + delta;

    ScopedInsertRows insertRows(q,  oldCount, newCount - 1);
    const auto range = std::make_pair(periods.crbegin() + oldCount, periods.crbegin() + newCount);

    for (auto chunk = range.first; chunk != range.second; ++chunk)
        m_data.push_front(*chunk);
}

} // namespace
} // namespace client
} // namespace nx
