// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../buffer.h"
#include "../byte_stream/pipeline.h"
#include "../thread/mutex.h"
#include "../thread/wait_condition.h"

namespace nx {
namespace utils {
namespace bstream {
namespace test {

class NX_UTILS_API NotifyingOutput:
    public nx::utils::bstream::AbstractOutput
{
public:
    virtual int write(const void* data, size_t count) override;

    void waitForReceivedDataToMatch(const nx::Buffer& referenceData);
    void waitForSomeDataToBeReceived();
    const nx::Buffer& internalBuffer() const;

private:
    nx::Buffer m_receivedData;
    nx::Mutex m_mutex;
    nx::WaitCondition m_cond;

    template<typename ConditionFunc>
    void waitFor(ConditionFunc func);
};

} // namespace test
} // namespace pipeline
} // namespace utils
} // namespace nx
