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
    m_isValid = parseXml(attributesXml);
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

bool HanwhaAttributes::parseXml(const QString& attributesXml)
{
    QXmlStreamReader reader(attributesXml);

    if (!reader.readNextStartElement())
        return false;

    if (reader.name() == kHanwhaAttributesNodeName)
    {
        if (!reader.readNextStartElement())
            return false;

        return parseGroups(reader);
    }

    if (reader.name() == kHanwhaGroupNodeName)
        return parseGroups(reader);

    return false;
}

bool HanwhaAttributes::parseGroups(QXmlStreamReader& reader)
{
    while (reader.name() == kHanwhaGroupNodeName)
    {
        auto group = reader.attributes().value(kHanwhaNameAttribute).toString();

        if (!reader.readNextStartElement())
            return false;

        if (!parseCategories(reader, group))
            return false;

        READ_NEXT_AND_RETURN_IF_NEEDED(reader);
    }

    return true;
}

bool HanwhaAttributes::parseCategories(
    QXmlStreamReader& reader,
    const QString& group)
{
    while (reader.name() == kHanwhaCategoryNodeName)
    {
        auto category = reader.attributes().value(kHanwhaNameAttribute);

        if (reader.readNextStartElement())
        {
            if (!parseChannelOrAttributes(reader, group))
                return false;
            READ_NEXT_AND_RETURN_IF_NEEDED(reader);
        }
        else
        {
            // It is an empty category section
            reader.readNext();
            if (reader.hasError())
                return false;
        }
    }

    return true;
}

bool HanwhaAttributes::parseChannelOrAttributes(
    QXmlStreamReader& reader,
    const QString& group)
{
    if (reader.name() == kHanwhaChannelNodeName)
    {
        bool success = false;
        auto channelNumber = reader.attributes()
            .value(kHanwhaChannelNumberAttribute)
            .toInt(&success);

        if (!success)
            return false;

        READ_NEXT_AND_RETURN_IF_NEEDED(reader);
        return parseAttributes(reader, group, channelNumber);
    }
    else if (reader.name() == kHanwhaAttributeNodeName)
    {
        return parseAttributes(reader, group, kNoChannel);
    }
    else
    {
        return true;
    }
}

bool HanwhaAttributes::parseAttributes(
    QXmlStreamReader& reader,
    const QString& group,
    int channel)
{
    while (reader.name() == kHanwhaAttributeNodeName)
    {
        auto attrs = reader.attributes();
        auto attrName = attrs.value(kHanwhaNameAttribute).toString();
        auto attrValue = attrs.value(kHanwhaValueAttribute).toString();

        m_attributes[channel][group][attrName] = attrValue;

        reader.skipCurrentElement();
        READ_NEXT_AND_RETURN_IF_NEEDED(reader);
    }

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
