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
    HikvisionMetadataMonitor* monitor,
    const nx::network::http::MultipartContentParser* parent)
    :
    m_manifest(manifest),
    m_monitor(monitor),
    m_parent(parent)
{
}

bool BytestreamFilter::processData(const QnByteArrayConstRef& buffer)
{
    const auto& headers = m_parent->prevFrameHeaders();
    if (const auto& it = headers.find("Content-Type"); it != headers.end())
    {
        const QString& contentType = it->second;
        if (contentType.startsWith("application/xml"))
        {
            NX_VERBOSE(this, "Got XML data:\n %1", buffer);

            const auto parsedEvent = AttributesParser::parseEventXml(buffer, m_manifest);
            if (parsedEvent)
                m_monitor->processEvent(*parsedEvent);
        }
        else
        {
            NX_VERBOSE(this, "Got content type %1, skipped", contentType);
        }
    }

    // MultipartContentParser is not designed to receive false from here.
    // After MultipartContentParser refactoring in 4.1 this function should return void.
    return true;
}

} // namespace hikvision
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
