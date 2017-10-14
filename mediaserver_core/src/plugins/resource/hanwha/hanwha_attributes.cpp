#if defined(ENABLE_HANWHA)

#include "hanwha_attributes.h"
#include "hanwha_common.h"
#include "hanwha_utils.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

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
    parseXml(reader, QString(), kNoChannel);
    m_isValid = !reader.hasError() || 
        reader.error() == QXmlStreamReader::PrematureEndOfDocumentError;
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

void HanwhaAttributes::parseXml(QXmlStreamReader& reader, const QString& group, int channel)
{
    while (!reader.atEnd() && reader.readNextStartElement())
    {
        if (reader.name() == kHanwhaAttributeNodeName)
            parseAttribute(reader, group, channel);
        if (reader.name() == kHanwhaGroupNodeName)
            parseXml(reader, reader.attributes().value(kHanwhaNameAttribute).toString(), channel);
        else if (reader.name() == kHanwhaChannelNodeName)
            parseXml(reader, group, reader.attributes().value("number").toInt());
        else
            parseXml(reader, group, channel);
    }
}

void HanwhaAttributes::parseAttribute(QXmlStreamReader& reader, const QString& group, int channel)
{
    const auto& attrs = reader.attributes();
    const auto attrName = attrs.value(kHanwhaNameAttribute).toString();
    const auto attrValue = attrs.value(kHanwhaValueAttribute).toString();
    m_attributes[channel][group][attrName] = attrValue;
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
