#pragma once

#if defined(ENABLED_HANWHA)

#include <QtCore/QString>
#include <QtCore/QXmlStreamReader>

#include <vector>

namespace nx {
namespace mediaserver {
namespace plugins {

class HanwhaAttributesParser
{
public:
    bool parseCgi(const QString& content);

    std::vector<QString> supportedEvents() const;
    bool readCgi(QXmlStreamReader& reader);
    bool readSubMenu(QXmlStreamReader& reader);
    bool readMonitorAction(QXmlStreamReader& reader);
    bool readChannelEvents(QXmlStreamReader& reader);
private:
    std::vector<QString> m_supportedEvents;
};

} // namespace plugins
} // namespace mediaserver
} // namespace nx

#endif // defined(ENABLED_HANWHA)
