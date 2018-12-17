#include <iostream>
#include <nx/utils/log/log_main.h>

#include "common.h"
#include "parser.h"
#include "bytestream_filter.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace dahua {

BytestreamFilter::BytestreamFilter(
    const EngineManifest& engineManifest,
    MetadataMonitor* metadataMonitor)
    :
    m_engineManifest(engineManifest),
    m_metadataMonitor(metadataMonitor)
{
}

bool BytestreamFilter::processData(const QnByteArrayConstRef& buffer)
{
    auto event = Parser::parseEventMessage(buffer, m_engineManifest);
    if (!event)
        return false;
    if (Parser::isHartbeatEvent(*event))
        return true;

    return m_metadataMonitor->processEvent(*event);
}

} // namespace dahua
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
