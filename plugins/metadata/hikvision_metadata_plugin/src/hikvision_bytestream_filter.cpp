#include "hikvision_bytestream_filter.h"
#include "hikvision_common.h"
#include "hikvision_string_helper.h"

#include <iostream>
#include "hikvision_attributes_parser.h"

namespace nx {
namespace mediaserver {
namespace plugins {

namespace {

static const QString kChannelField = lit("channel");
static const QString kRegionField = lit("regionid");

static const QString kActive = lit("true");
static const QString kInactive = lit("false");

} // namespace

HikvisionBytestreamFilter::HikvisionBytestreamFilter(
    const Hikvision::DriverManifest& manifest,
    HikvisionBytestreamFilter::Handler handler)
    :
    m_manifest(manifest),
    m_handler(handler)
{
}

HikvisionBytestreamFilter::~HikvisionBytestreamFilter()
{

}

bool HikvisionBytestreamFilter::processData(const QnByteArrayConstRef& buffer)
{
    NX_ASSERT(m_handler, lit("No handler is set."));
    if (!m_handler)
        return false;

    bool success = false;
    using namespace nx::mediaserver::plugins::hikvision;
    auto hikvisionEvent = AttributesParser::parseEventXml(buffer, m_manifest, &success);
    if (!success)
        return false;

    std::vector<HikvisionEvent> result;
    result.push_back(hikvisionEvent);
    m_handler(result);
    return true;
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx
