// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <limits>

#include <api/helpers/request_helpers_fwd.h>
#include <api/helpers/multiserver_request_data.h>
#include <api/helpers/event_log_request_data.h>
#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>

#include <nx/utils/uuid.h>
#include <nx/vms/event/event_fwd.h>

struct NX_VMS_COMMON_API QnEventLogMultiserverRequestData: public QnMultiserverRequestData
{
    QnEventLogMultiserverRequestData();
    QnEventLogMultiserverRequestData(
        QnResourcePool* resourcePool, const nx::network::rest::Params& params);

    virtual void loadFromParams(
        QnResourcePool* resourcePool, const nx::network::rest::Params& params) override;

    virtual nx::network::rest::Params toParams() const override;
    virtual bool isValid() const override;

    QnEventLogFilterData filter;
    Qt::SortOrder order = kDefaultOrder;
    int limit = kNoLimit;

    static constexpr int kNoLimit = std::numeric_limits<decltype(limit)>::max();
    static constexpr Qt::SortOrder kDefaultOrder = Qt::AscendingOrder;
};
