#pragma once

#include <vector>

#include <QtCore/QString>
#include <QtCore/QXmlStreamReader>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
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
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
