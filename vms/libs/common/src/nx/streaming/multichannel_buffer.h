#pragma once

#include <map>
#include <chrono>
#include <atomic>
#include <deque>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/streaming/media_data_packet.h>

namespace nx {
namespace streaming {

class MultiChannelBuffer
{
public:
    using OrderedChannelQueues = std::vector<std::deque<QnAbstractMediaDataPtr>>;

public:
    MultiChannelBuffer(int videoChannelCount, int audioChannelCount);

    void pushData(const QnAbstractMediaDataPtr& media);
    QnAbstractMediaDataPtr nextData(const std::chrono::milliseconds& maxWait);

    bool canAcceptData() const;
    void terminate();

private:
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_wait;

    int m_videoChannelCount;
    OrderedChannelQueues m_mediaData;

    std::atomic<bool> m_terminated{false};
    std::chrono::milliseconds m_minSharedDuration{200};
    bool m_needToRefillBuffer{true};
};

} // namespace streaming
} // namespace nx
