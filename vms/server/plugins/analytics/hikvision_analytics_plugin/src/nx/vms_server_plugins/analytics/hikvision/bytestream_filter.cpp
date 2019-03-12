#include "bytestream_filter.h"

#include <nx/utils/log/log_main.h>

#include "common.h"
#include "attributes_parser.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hikvision {

BytestreamFilter::BytestreamFilter(
    const Hikvision::EngineManifest& manifest,
    HikvisionMetadataMonitor* monitor)
    :
    m_manifest(manifest),
    m_monitor(monitor)
{
}

bool BytestreamFilter::processData(const QnByteArrayConstRef& buffer)
{
    NX_VERBOSE(this, lm("Got XML data:\n %1").arg(buffer));

    auto hikvisionEvent = AttributesParser::parseEventXml(buffer, m_manifest);
    if (!hikvisionEvent)
        return false;
    return m_monitor->processEvent(*hikvisionEvent);
}

} // namespace hikvision
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
