#pragma once

#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QXmlStreamReader>

#include <vector>

#include "common.h"
#include "geometry.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hikvision {

class AttributesParser
{
public:
    static std::optional<std::vector<QString>> parseSupportedEventsXml(const QByteArray& content);
    static std::optional<EventWithRegions> parseEventXml(
        const QByteArray& content,
        const Hikvision::EngineManifest& manifest);

    static std::vector<HikvisionEvent> parseLprXml(
        const QByteArray& content,
        const Hikvision::EngineManifest& manifest);
private:
    static std::vector<Region> parseRegionList(QXmlStreamReader& reader);
    static Region parseRegionEntry(QXmlStreamReader& reader);
    static std::vector<Point> parseCoordinatesList(QXmlStreamReader& reader);

    static HikvisionEvent parsePlateData(
        QXmlStreamReader& reader,
        const Hikvision::EngineManifest& manifest);
};

} // namespace hikvision
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
