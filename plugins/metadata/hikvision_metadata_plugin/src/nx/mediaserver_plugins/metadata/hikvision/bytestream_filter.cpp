#include "bytestream_filter.h"
#include "common.h"
#include "string_helper.h"

#include <iostream>
#include "attributes_parser.h"
#include <nx/utils/log/log_main.h>

namespace nx {
namespace mediaserver {
namespace plugins {
namespace hikvision {

BytestreamFilter::BytestreamFilter(
    const Hikvision::DriverManifest& manifest,
    HikvisionMetadataMonitor* monitor)
    :
    m_manifest(manifest),
    m_monitor(monitor)
{
}

bool BytestreamFilter::processData(const QnByteArrayConstRef& buffer)
{
    NX_VERBOSE(this, lm("Got XML data:\n %1").arg(buffer));

    using namespace nx::mediaserver::plugins::hikvision;
    auto hikvisionEvent = AttributesParser::parseEventXml(buffer, m_manifest);
    if (!hikvisionEvent)
        return false;
    return m_monitor->processEvent(*hikvisionEvent);
}

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver
} // namespace nx
