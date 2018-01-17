#pragma once

#include <limits>

#include <api/helpers/request_helpers_fwd.h>
#include <api/helpers/multiserver_request_data.h>
#include <api/helpers/event_log_request_data.h>
#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>
#include <utils/common/request_param.h>

#include <nx/utils/uuid.h>
#include <nx/vms/event/event_fwd.h>

struct QnEventLogMultiserverRequestData: public QnMultiserverRequestData
{
    QnEventLogMultiserverRequestData();
    QnEventLogMultiserverRequestData(QnResourcePool* resourcePool, const QnRequestParamList& params);

    virtual void loadFromParams(QnResourcePool* resourcePool,
        const QnRequestParamList& params) override;
    virtual void loadFromParams(QnResourcePool* resourcePool,
        const QnRequestParams& params) override;

    virtual QnRequestParamList toParams() const override;
    virtual bool isValid() const override;

    QnEventLogFilterData filter;
    Qt::SortOrder order = kDefaultOrder;
    int limit = kNoLimit;

    static constexpr int kNoLimit = std::numeric_limits<decltype(limit)>::max();
    static constexpr Qt::SortOrder kDefaultOrder = Qt::AscendingOrder;
};
