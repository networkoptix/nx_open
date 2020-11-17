#pragma once

#include "metadata_monitor.h"
#include "common.h"

#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>
#include <nx/network/http/multipart_content_parser.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hikvision {

class MetadataMonitor;

class BytestreamFilter: public nx::utils::bstream::AbstractByteStreamFilter
{
public:
    BytestreamFilter(
        const Hikvision::EngineManifest& manifest,
        HikvisionMetadataMonitor* monitor,
        const nx::network::http::MultipartContentParser* parent);
    virtual ~BytestreamFilter() = default;
    virtual bool processData(const QnByteArrayConstRef& notification) override;

    bool processEvent(const HikvisionEvent hikvisionEvent);
private:
    void addExpiredEvents(std::vector<HikvisionEvent>& result);
private:
    const Hikvision::EngineManifest m_manifest;
    HikvisionMetadataMonitor* m_monitor = nullptr;
    const nx::network::http::MultipartContentParser* m_parent = nullptr;
};

} // namespace hikvision
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
