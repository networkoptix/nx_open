#pragma once

#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QXmlStreamReader>

#include <vector>

#include "common.h"

namespace nx {
namespace mediaserver {
namespace plugins {
namespace hikvision {

class AttributesParser
{
public:
    static boost::optional<std::vector<QString>> parseSupportedEventsXml(const QByteArray& content);
    static boost::optional<HikvisionEvent> parseEventXml(
        const QByteArray& content,
        const Hikvision::DriverManifest manifest);
};

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver
} // namespace nx
