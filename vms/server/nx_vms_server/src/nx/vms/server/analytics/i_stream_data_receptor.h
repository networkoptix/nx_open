#pragma once

#include <set>

#include <nx/streaming/abstract_data_packet.h>
#include <nx/vms/server/analytics/stream_requirements.h>

namespace nx::vms::server::analytics {

class StreamDataReceptor
{
public:
    virtual ~StreamDataReceptor() = default;

    virtual void putData(const QnAbstractDataPacketPtr& streamData) = 0;

    virtual StreamProviderRequirements providerRequirements(
        nx::vms::api::StreamIndex streamIndex) const = 0;

    virtual void registerStream(nx::vms::api::StreamIndex streamIndex) = 0;
};

} // namespace nx::vms::server::analytics
