#if defined(ENABLE_HANWHA)

#include "hanwha_attributes.h"
#include "hanwha_common.h"
#include "hanwha_utils.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

struct HanwhaAttributes::XmlElement
{
    XmlElement(const QStringRef& name, const QXmlStreamAttributes& attributes):
        name(name), attributes(attributes)
    {
    }

    QStringRef name;
    QXmlStreamAttributes attributes;
};

HanwhaAttributes::HanwhaAttributes(nx_http::StatusCode::Value statusCode):
    m_statusCode(statusCode)
{
}

HanwhaAttributes::HanwhaAttributes(
    const QString& attributesXml,
    nx_http::StatusCode::Value statusCode)
    :
    m_statusCode(statusCode)
{
    QXmlStreamReader reader(attributesXml);
    parseXml(reader, XmlElementList());
    m_isValid = !reader.hasError();
}

bool HanwhaAttributes::isValid() const
{
    return m_isValid;
}

nx_http::StatusCode::Value HanwhaAttributes::statusCode() const
{
    return m_statusCode;
}

boost::optional<QString> HanwhaAttributes::findAttribute(
    const QString& group,
    const QString& attributeName,
    int channel) const
{
    auto channelMap = m_attributes.find(channel);
    if (channelMap == m_attributes.cend())
        return boost::none;

    auto groupMap = channelMap->second.find(group);
    if (groupMap == channelMap->second.cend())
        return boost::none;

    auto attribute = groupMap->second.find(attributeName);
    if (attribute == groupMap->second.cend())
        return boost::none;

    return attribute->second;
}

void HanwhaAttributes::parseXml(QXmlStreamReader& reader, XmlElementList path)
{
    while (!reader.atEnd() && reader.readNextStartElement())
    {
        if (reader.name() == kHanwhaAttributeNodeName)
            parseAttribute(reader, path);
        else
            parseXml(reader, path << XmlElement(reader.name(), reader.attributes()));
        reader.readNext();
    }
}

bool HanwhaAttributes::parseAttribute(QXmlStreamReader& reader, XmlElementList path)
{
    QString group;
    int channel = kNoChannel;
    for (const auto& element: path)
    {
        if (element.name == "group")
            group = element.attributes.value("name").toString();
        else if (element.name == "channel")
            channel = element.attributes.value("number").toInt();
    }
    if (group.isEmpty())
        return false; //< Ignore unknown attributes placed not in a group.

    const auto& attrs = reader.attributes();
    const auto attrName = attrs.value(kHanwhaNameAttribute).toString();
    const auto attrValue = attrs.value(kHanwhaValueAttribute).toString();
    m_attributes[channel][group][attrName] = attrValue;
    return true;
}

template<>
boost::optional<bool> HanwhaAttributes::attribute<bool>(
    const QString& group,
    const QString& attributeName,
    int channel) const
{
    return toBool(findAttribute(group, attributeName, channel));   
}

template<>
boost::optional<QString> HanwhaAttributes::attribute<QString>(
    const QString& group,
    const QString& attributeName,
    int channel) const
{
    return findAttribute(group, attributeName, channel);
}

template<>
boost::optional<int> HanwhaAttributes::attribute<int>(
    const QString& group,
    const QString& attributeName,
    int channel) const
{
    return toInt(findAttribute(group, attributeName, channel));
}

template<>
boost::optional<double> HanwhaAttributes::attribute<double>(
    const QString& group,
    const QString& attributeName,
    int channel) const
{
    return toDouble(findAttribute(group, attributeName, channel));
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
