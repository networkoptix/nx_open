#include "bytestream_filter.h"

#include "common.h"
#include "parser.h"

namespace nx::vms_server_plugins::analytics::dahua {

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
    if (Parser::isHeartbeatEvent(*event))
        return true;

    return m_metadataMonitor->processEvent(*event);
}

} // namespace nx::vms_server_plugins::analytics::dahua
