#pragma once

#include <nx/utils/move_only_func.h>

#include "abstract_data_receptor.h"

class ConditionalDataProxy:
    public QnAbstractDataReceptor
{
public:
    using ConditionFunc =
        nx::utils::MoveOnlyFunc<bool(const QnAbstractDataPacketPtr& /*data*/)>;

    ConditionalDataProxy(
        QnAbstractDataReceptorPtr target,
        ConditionFunc condition);

    virtual bool canAcceptData() const override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

private:
    QnAbstractDataReceptorPtr m_target;
    ConditionFunc m_conditionFunc;
};
