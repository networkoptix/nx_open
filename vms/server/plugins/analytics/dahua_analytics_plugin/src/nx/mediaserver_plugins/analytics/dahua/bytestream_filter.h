#pragma once

#include <plugins/plugin_api.h>
#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>
#include <nx/utils/elapsed_timer.h>

#include "metadata_monitor.h"
#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace dahua {

class MetadataMonitor;

class BytestreamFilter: public nx::utils::bstream::AbstractByteStreamFilter
{
public:
    BytestreamFilter(const EngineManifest& manifest, MetadataMonitor* monitor);
    virtual ~BytestreamFilter() = default;
    virtual bool processData(const QnByteArrayConstRef& notification) override;

private:
    const EngineManifest m_engineManifest;
    MetadataMonitor* m_monitor = nullptr;
};

} // namespace dahua
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
