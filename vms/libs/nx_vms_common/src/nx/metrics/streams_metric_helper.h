// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>

#include <nx/vms/api/types/motion_types.h>

namespace nx::metric { struct Storage; }

namespace nx::vms::metric {

    class NX_VMS_COMMON_API StreamMetricHelper
    {
    public:
        StreamMetricHelper(nx::metric::Storage* metrics);
        ~StreamMetricHelper();

        void setStream(nx::vms::api::StreamIndex stream);
    private:
        std::atomic<qint64>* getMetric(nx::vms::api::StreamIndex index);
    private:
        nx::metric::Storage* m_metrics = nullptr;
        nx::vms::api::StreamIndex m_stream{nx::vms::api::StreamIndex::undefined};
    };
} // namespace nx::vms::metric
