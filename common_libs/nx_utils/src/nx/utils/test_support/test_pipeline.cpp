#include "test_pipeline.h"

namespace nx {
namespace utils {
namespace bstream {
namespace test {

int NotifyingOutput::write(const void* data, size_t count)
{
    NX_ASSERT(count <= std::numeric_limits<int>::max());

    QnMutexLocker lock(&m_mutex);
    m_receivedData.append(static_cast<const char*>(data), (int)count);
    m_cond.wakeAll();
    return (int)count;
}

void NotifyingOutput::waitForReceivedDataToMatch(const QByteArray& referenceData)
{
    waitFor(
        [this, &referenceData]()
        {
            return m_receivedData.startsWith(referenceData);
        });
}

void NotifyingOutput::waitForSomeDataToBeReceived()
{
    waitFor([this]() { return !m_receivedData.isEmpty(); });
}

const QByteArray& NotifyingOutput::internalBuffer() const
{
    return m_receivedData;
}

template<typename ConditionFunc>
void NotifyingOutput::waitFor(ConditionFunc func)
{
    QnMutexLocker lock(&m_mutex);
    while (!func())
        m_cond.wait(lock.mutex());
}

} // namespace test
} // namespace pipeline
} // namespace utils
} // namespace nx
