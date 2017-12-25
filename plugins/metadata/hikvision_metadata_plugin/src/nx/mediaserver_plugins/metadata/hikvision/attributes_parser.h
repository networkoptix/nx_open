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
    static std::vector<QString> parseSupportedEventsXml(const QByteArray& content, bool* outSuccess);
    static HikvisionEvent parseEventXml(
        const QByteArray& content,
        const Hikvision::DriverManifest manifest,
        bool* outSuccess);
};

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver
} // namespace nx
