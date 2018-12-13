#include "bytestream_filter.h"
#include "common.h"
#include "string_helper.h"

#include <iostream>
#include "attributes_parser.h"
#include <nx/utils/log/log_main.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace dahua {

BytestreamFilter::BytestreamFilter(
    const Dahua::EngineManifest& manifest,
    MetadataMonitor* monitor)
    :
    m_engineManifest(manifest),
    m_monitor(monitor)
{
}

bool BytestreamFilter::processData(const QnByteArrayConstRef& buffer)
{
    auto event = AttributesParser::parseEventMessage(buffer, m_engineManifest);
    if (!event)
        return false;
    if (AttributesParser::isHartbeatEvent(*event))
        return true;

    return m_monitor->processEvent(*event);
}

} // namespace dahua
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
