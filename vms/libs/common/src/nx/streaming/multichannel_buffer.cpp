#include "multichannel_buffer.h"

#include <nx/utils/std/optional.h>
#include <nx/utils/elapsed_timer.h>

namespace nx {
namespace streaming {

using namespace std::chrono;
using namespace std::literals::chrono_literals;

namespace {

static milliseconds kIncreaseSharedDurationStep = 200ms;
static milliseconds kMaxQueueDuration = 1s;
static int kMaxQueueSize = 200;

std::optional<int> nextDataQueueIndex(
    const MultiChannelBuffer::OrderedChannelQueues& queues,
    const milliseconds& minQueueDuration)
{
    std::optional<int> result;
    auto minTimestamp = std::numeric_limits<int64_t>::max();
    bool anyQueueHitSizeLimit = false;
    for (const auto& queue: queues)
    {
        if (queue.size() >= kMaxQueueSize)
        {
            anyQueueHitSizeLimit = true;
            break;
        }
    }

    for (auto i = 0; i < queues.size(); ++i)
    {
        const auto& queue = queues[i];
        if (queue.empty())
        {
            if (anyQueueHitSizeLimit)
                continue;

            return std::nullopt;
        }

        const auto minQueueTimestamp = (*queue.cbegin())->timestamp;
        if (minQueueDuration > milliseconds::zero())
        {
            const auto maxQueueTimestamp = (*queue.crbegin())->timestamp;
            const microseconds duration(maxQueueTimestamp - minQueueTimestamp);
            if (duration < minQueueDuration)
                return std::nullopt;
        }

        if (minQueueTimestamp < minTimestamp)
        {
            minTimestamp = minQueueTimestamp;
            result = i;
        }
    }

    return result;
}

} // namespace

MultiChannelBuffer::MultiChannelBuffer(int videoChannelCount, int audioChannelCount):
    m_videoChannelCount(videoChannelCount),
    m_mediaData(videoChannelCount + audioChannelCount)
{
}

void MultiChannelBuffer::pushData(const QnAbstractMediaDataPtr& media)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_terminated.load())
            return;

        const auto channel = media->dataType == QnAbstractMediaData::DataType::VIDEO
            ? media->channelNumber
            : m_videoChannelCount + media->channelNumber;

        if (channel < 0 || channel >= m_mediaData.size())
            return;

        if (m_mediaData.size() >= kMaxQueueSize)
            return;

        m_mediaData[channel].push_back(media);
    }

    m_wait.wakeOne();
}

QnAbstractMediaDataPtr MultiChannelBuffer::nextData(const milliseconds& maxWait)
{
    QnMutexLocker lock(&m_mutex);
    std::optional<int> queueIndex;
    const auto sharedDuration = m_needToRefillBuffer ? m_minSharedDuration : milliseconds::zero();
    nx::utils::ElapsedTimer timer;
    timer.restart();
    while (
        (queueIndex = nextDataQueueIndex(m_mediaData, sharedDuration)) == std::nullopt
        && timer.elapsed() < maxWait
        && !m_terminated.load())
    {
        const auto maxWaitDuration = (maxWait - timer.elapsed()).count();
        m_wait.wait(&m_mutex, maxWaitDuration > 0 ? maxWaitDuration : 0);
    }

    if (queueIndex == std::nullopt)
    {
        m_needToRefillBuffer = true;
        m_minSharedDuration += kIncreaseSharedDurationStep;
        return nullptr;
    }

    if (m_terminated.load())
        return nullptr;

    m_needToRefillBuffer = false;
    auto& queue = m_mediaData[*queueIndex];
    auto data = queue.front();
    queue.pop_front();
    return data;
}

bool MultiChannelBuffer::canAcceptData() const
{
    return true;
}

void MultiChannelBuffer::terminate()
{
    m_terminated.store(true);
    m_wait.wakeAll();
}

} // namespace streaming
} // namespace nx
