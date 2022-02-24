// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_pipeline.h"

#include <nx/utils/string.h>

namespace nx {
namespace utils {
namespace bstream {
namespace test {

int NotifyingOutput::write(const void* data, size_t count)
{
    NX_ASSERT(count <= std::numeric_limits<int>::max());

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_receivedData.append(static_cast<const char*>(data), (int)count);
    m_cond.wakeAll();
    return (int)count;
}

void NotifyingOutput::waitForReceivedDataToMatch(const nx::Buffer& referenceData)
{
    waitFor(
        [this, &referenceData]()
        {
            return m_receivedData.starts_with(referenceData);
        });
}

void NotifyingOutput::waitForSomeDataToBeReceived()
{
    waitFor([this]() { return !m_receivedData.empty(); });
}

const nx::Buffer& NotifyingOutput::internalBuffer() const
{
    return m_receivedData;
}

template<typename ConditionFunc>
void NotifyingOutput::waitFor(ConditionFunc func)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    while (!func())
        m_cond.wait(lock.mutex());
}

} // namespace test
} // namespace pipeline
} // namespace utils
} // namespace nx
