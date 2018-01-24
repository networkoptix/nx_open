#pragma once

#include <boost/optional/optional.hpp>

#include "metadata_monitor.h"
#include "common.h"

#include <plugins/plugin_api.h>
#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>
#include <nx/utils/elapsed_timer.h>

namespace nx {
namespace mediaserver {
namespace plugins {
namespace hikvision {

class MetadataMonitor;

class BytestreamFilter: public nx::utils::bstream::AbstractByteStreamFilter
{
public:
    BytestreamFilter(const Hikvision::DriverManifest& manifest, HikvisionMetadataMonitor* monitor);
    virtual ~BytestreamFilter() = default;
    virtual bool processData(const QnByteArrayConstRef& notification) override;

    bool processEvent(const HikvisionEvent hikvisionEvent);
private:
    void addExpiredEvents(std::vector<HikvisionEvent>& result);
private:
    const Hikvision::DriverManifest m_manifest;
    HikvisionMetadataMonitor* m_monitor;
};

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver
} // namespace nx
