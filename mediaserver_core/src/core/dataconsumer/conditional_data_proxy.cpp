#include "conditional_data_proxy.h"

ConditionalDataProxy::ConditionalDataProxy(
    QnAbstractDataReceptorPtr target,
    ConditionFunc condition)
    :
    m_target(target),
    m_conditionFunc(std::move(condition))
{
}

bool ConditionalDataProxy::canAcceptData() const
{
    return m_target->canAcceptData();
}

void ConditionalDataProxy::putData(const QnAbstractDataPacketPtr& data)
{
    if (m_conditionFunc(data))
        m_target->putData(data);
}
