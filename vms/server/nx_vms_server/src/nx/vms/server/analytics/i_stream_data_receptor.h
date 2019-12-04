#pragma once

#include <set>

#include <nx/streaming/abstract_data_packet.h>
#include <nx/vms/api/analytics/stream_type.h>

namespace nx::vms::server::analytics {

class IStreamDataReceptor
{
public:
    virtual ~IStreamDataReceptor() = default;

    virtual void putData(const QnAbstractDataPacketPtr& streamData) = 0;

    virtual nx::vms::api::analytics::StreamTypes requiredStreamTypes() const = 0;
};

} // namespace nx::vms::server::analytics
