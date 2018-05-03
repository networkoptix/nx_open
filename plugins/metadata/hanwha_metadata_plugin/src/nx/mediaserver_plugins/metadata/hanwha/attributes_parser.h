#pragma once

#include <vector>

#include <QtCore/QString>
#include <QtCore/QXmlStreamReader>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace hanwha {

class AttributesParser
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

} // namespace hanwha
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
