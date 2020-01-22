#include "put_in_order_data_provider.h"

namespace nx::vms::server {

using namespace std::chrono;

static const float kExtraBufferSizeCoeff = 1.5;
static std::chrono::microseconds kJitterAggregationPeriod = 1s;

static const std::chrono::milliseconds kMinExtraBufferSize = 34ms;

static milliseconds toMs(microseconds value)
{
    return duration_cast<milliseconds>(value);
}

// ------------------------- ProxyDataProvider -------------------------------

ProxyDataProvider::ProxyDataProvider(const QnAbstractStreamDataProviderPtr& provider):
    QnAbstractStreamDataProvider(provider->getResource()),
    m_provider(provider)
{
}

bool ProxyDataProvider::dataCanBeAccepted() const
{
    return m_provider->dataCanBeAccepted();
}

bool ProxyDataProvider::isReverseMode() const
{
    return m_provider->isReverseMode();
}

void ProxyDataProvider::disconnectFromResource()
{
    return m_provider->disconnectFromResource();
}

void ProxyDataProvider::setRole(Qn::ConnectionRole role)
{
    m_provider->setRole(role);
}

QnConstResourceVideoLayoutPtr ProxyDataProvider::getVideoLayout() const
{
    return m_provider->getVideoLayout();
}

bool ProxyDataProvider::hasVideo() const
{
    return m_provider->hasVideo();
}

bool ProxyDataProvider::needConfigureProvider() const
{
    NX_MUTEX_LOCKER mutex(&m_mutex);
    for (auto processor: m_dataprocessors)
    {
        if (processor->needConfigureProvider())
            return true;
    }
    return false;
}

void ProxyDataProvider::startIfNotRunning()
{
    m_provider->startIfNotRunning();
}

QnSharedResourcePointer<QnAbstractVideoCamera> ProxyDataProvider::getOwner() const
{
    return m_provider->getOwner();
}

// ------------------- AbstractPutInOrderDataProvider -------------------------

AbstractDataReorderer::AbstractDataReorderer(const Settings& settings):
    m_queueDuration(std::max(settings.initialQueueDuration, settings.minQueueDuration)),
    m_settings(settings)
{
    m_timer.restart();
}

std::optional<std::chrono::microseconds> AbstractDataReorderer::flush()
{
    NX_MUTEX_LOCKER lock(& m_mutex);
    std::optional<std::chrono::microseconds> lastFlushTime;
    while (!m_dataQueue.empty())
    {
        auto data = std::move(m_dataQueue.front());
        flushData(data);
        lastFlushTime = microseconds(data->timestamp);
        m_dataQueue.pop_front();
    }
    return lastFlushTime;
}

void AbstractDataReorderer::processNewData(const QnAbstractDataPacketPtr& data)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto media = std::dynamic_pointer_cast<QnAbstractMediaData>(data);
    if (!media)
        return;

    updateBufferSize(data);

    const auto itr = std::upper_bound(m_dataQueue.begin(), m_dataQueue.end(), media,
        [](const QnAbstractMediaDataPtr& left, const QnAbstractMediaDataPtr& right)
        {
            return left->timestamp < right->timestamp;
        });

    m_dataQueue.insert(itr, media);

    const auto keepDataToTime = (*m_dataQueue.rbegin())->timestamp - m_queueDuration.count();
    while (!m_dataQueue.empty() && (*m_dataQueue.begin())->timestamp <= keepDataToTime)
    {
        auto data = std::move(m_dataQueue.front());
        flushData(data);
        m_dataQueue.pop_front();
    }
}

void AbstractDataReorderer::updateBufferSize(const QnAbstractDataPacketPtr& data)
{
    auto multiply = [](microseconds value, float coeff)
    {
        return microseconds(int(value.count() * coeff));
    };

    m_lastTimeUs = std::max(data->timestamp, m_lastTimeUs);
    const microseconds jitter = microseconds(std::max(0LL, m_lastTimeUs - data->timestamp));
    if (jitter == microseconds::zero())
        return;

    const microseconds currentTime = m_timer.elapsed();
    if (!m_lastJitter.empty() &&
        currentTime - m_lastJitter.last().timestamp < kJitterAggregationPeriod)
    {
        if (jitter > m_lastJitter.last().jitter)
        {
            m_lastJitter.last().jitter = jitter;
            m_lastJitter.update();
        }
    }
    else
    {
        m_lastJitter.push_back({currentTime, jitter});
    }

    bool isJitterQueueFull = false;
    while (m_lastJitter.front().timestamp < currentTime - kJitterBufferSize)
    {
        isJitterQueueFull = true;
        m_lastJitter.pop_front();
    }

    const microseconds maxJitter= m_lastJitter.top().jitter;
    NX_VERBOSE(this, "Time=%1, jitter: %2, maxJitter=%3",
        toMs(microseconds(data->timestamp)), toMs(jitter), toMs(maxJitter));

    const auto keepDataToTime = m_lastTimeUs - m_queueDuration.count();
    if (data->timestamp < keepDataToTime)
    {
        // Increase queue size if need
        auto newValue = std::min(multiply(jitter, kExtraBufferSizeCoeff), m_settings.maxQueueDuration);
        if (m_queueDuration != newValue)
        {
            NX_DEBUG(this, "Increase queue duration from %1 to %2", toMs(m_queueDuration), toMs(newValue));
                m_queueDuration = newValue;
        }
    }

    const auto testJitter = multiply(maxJitter, kExtraBufferSizeCoeff);
    if (isJitterQueueFull
        && m_settings.policy == BufferingPolicy::increaseAndDescrease
        && testJitter < m_queueDuration)
    {
        auto newValue = std::max(testJitter, m_settings.minQueueDuration);
        if (m_queueDuration != newValue)
        {
            NX_DEBUG(this, "decrease queue duration from %1 to %2. History size %3",
                toMs(m_queueDuration), toMs(newValue),
                toMs(m_lastJitter.last().timestamp - m_lastJitter.front().timestamp));
            m_queueDuration = newValue;
        }
    }
}

// ----------------------- PutInOrderDataProvider --------------------------------------

PutInOrderDataProvider::PutInOrderDataProvider(
    const QnAbstractStreamDataProviderPtr& provider, const Settings& settings)
    :
    ProxyDataProvider(provider),
    AbstractDataReorderer(settings)
{
    start();
}

PutInOrderDataProvider::~PutInOrderDataProvider()
{
    stop();
}

void PutInOrderDataProvider::start(Priority /*priority*/)
{
    // This class hasn't own thread.
    m_provider->addDataProcessor(this);
}

void PutInOrderDataProvider::stop()
{
    m_provider->removeDataProcessor(this);
}

void PutInOrderDataProvider::flushData(const QnAbstractDataPacketPtr& data)
{
    ProxyDataProvider::putData(data);
}

void PutInOrderDataProvider::putData(const QnAbstractDataPacketPtr& data)
{
    processNewData(data);
}

// ----------------------- SimpleReorderingProvider --------------------------------------

void SimpleReorderer::flushData(const QnAbstractDataPacketPtr& data)
{
    m_queue.push_back(data);
}

std::deque<QnAbstractDataPacketPtr>& SimpleReorderer::queue() 
{ 
    return m_queue; 
}

} // namespace nx::vms::server
