#pragma once

#include <nx/vms/api/types_fwd.h>
#include <nx/vms/api/types/motion_types.h>

namespace nx::metrics { struct Storage; }

namespace nx::vms::metrics {

    class StreamMetricHelper
    {
    public:
        StreamMetricHelper(nx::metrics::Storage* metrics);
        ~StreamMetricHelper();

        void setStream(nx::vms::api::StreamIndex stream);
    private:
        std::atomic<qint64>* getMetric(nx::vms::api::StreamIndex index);
    private:
        nx::metrics::Storage* m_metrics = nullptr;
        nx::vms::api::StreamIndex m_stream{nx::vms::api::StreamIndex::undefined};
    };
} // namespace nx::vms::metrics
