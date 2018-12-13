#pragma once

#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QXmlStreamReader>

#include <vector>

#include <nx/utils/std/optional.h>

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace dahua {

class AttributesParser
{
public:
    static std::vector<QString> parseSupportedEventsMessage(const QByteArray& content);

    static std::optional<DahuaEvent> parseEventMessage(const QByteArray& content,
        const Dahua::EngineManifest& manifest);

    static bool isHartbeatEvent(const DahuaEvent& event);
};

} // namespace dahua
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
