#pragma once

#include "../byte_stream/pipeline.h"
#include "../thread/mutex.h"
#include "../thread/wait_condition.h"

namespace nx {
namespace utils {
namespace bstream {
namespace test {

class NX_UTILS_API NotifyingOutput:
    public utils::bstream::AbstractOutput
{
public:
    virtual int write(const void* data, size_t count) override;

    void waitForReceivedDataToMatch(const QByteArray& referenceData);
    void waitForSomeDataToBeReceived();
    const QByteArray& internalBuffer() const;

private:
    QByteArray m_receivedData;
    QnMutex m_mutex;
    QnWaitCondition m_cond;

    template<typename ConditionFunc>
    void waitFor(ConditionFunc func);
};

} // namespace test
} // namespace pipeline
} // namespace utils
} // namespace nx
