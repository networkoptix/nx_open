#include "hanwha_cgi_parameters.h"
#include "hanwha_common.h"

namespace nx {
namespace mediaserver_core {
namespace plugins {

HanwhaCgiParameters::HanwhaCgiParameters(nx_http::StatusCode::Value statusCode):
    m_statusCode(statusCode)
{

}

HanwhaCgiParameters::HanwhaCgiParameters(
    const nx::Buffer& rawBuffer,
    nx_http::StatusCode::Value statusCode)
    :
    m_statusCode(statusCode)

{
    m_isValid = parseXml(rawBuffer);
}

boost::optional<HanwhaCgiParameter> HanwhaCgiParameters::parameter(
    const QString& submenu,
    const QString& action,
    const QString& parameter) const
{
    auto submenuItr = m_parameters.find(submenu);
    if (submenuItr == m_parameters.cend())
        return boost::none;

    auto actionItr = submenuItr->second.find(action);
    if (actionItr == submenuItr->second.cend())
        return boost::none;

    auto parameterItr = actionItr->second.find(parameter);
    if (parameterItr == actionItr->second.cend())
        return boost::none;

    return parameterItr->second;
}

boost::optional<HanwhaCgiParameter> HanwhaCgiParameters::parameter(const QString& path) const
{
    auto split = path.split("/");
    if (split.size() != 3)
        return boost::none;

    return parameter(split[0], split[1], split[2]);
}

bool HanwhaCgiParameters::isValid() const
{
    return m_isValid && nx_http::StatusCode::isSuccessCode(m_statusCode);
}

nx_http::StatusCode::Value HanwhaCgiParameters::statusCode() const
{
    return m_statusCode;
}

bool HanwhaCgiParameters::parseXml(const nx::Buffer& rawBuffer)
{
    QXmlStreamReader reader(rawBuffer);

    if (!reader.readNextStartElement())
        return false;

    auto name  = reader.name();
    if (name == kHanwhaCgiNodeName)
        reader.readNextStartElement();

    if (reader.name() != kHanwhaSubmenuNodeName)
        return false;

    return parseSubmenus(reader);
}

bool HanwhaCgiParameters::parseSubmenus(QXmlStreamReader& reader)
{
    while (reader.name() == kHanwhaSubmenuNodeName)
    {
        auto submenuName = reader.attributes().value(kHanwhaNameAttribute).toString();

        if (!reader.readNextStartElement())
            return false;

        if (!parseActions(reader, submenuName))
            return false;

        reader.readNextStartElement();
    }

    return true;
}

bool HanwhaCgiParameters::parseActions(QXmlStreamReader& reader, const QString& submenu)
{
    while (reader.name() == kHanwhaActionNodeName)
    {
        auto actionName = reader.attributes().value(kHanwhaNameAttribute).toString();
        actionName.replace(L'/', L'_');

        reader.readNext();
        if (reader.isEndElement())
        {
            reader.readNextStartElement();
            continue;
        }

        if (reader.name() == kHanwhaParameterNodeName)
        {
            if (!parseParameters(reader, submenu, actionName))
                return false;
        }
        reader.readNextStartElement();
    }

    return true;
}

bool HanwhaCgiParameters::parseParameters(
    QXmlStreamReader& reader,
    const QString& submenu,
    const QString& action)
{
    while (reader.name() == kHanwhaParameterNodeName)
    {
        const auto attributes = reader.attributes();
        const auto parameterName = attributes.value(kHanwhaNameAttribute).toString();
        const bool isRequestParameter = attributes
            .value(kHanwhaParameterIsRequestAttribute)
            .toString()
            .toLower() == kHanwhaTrue.toLower();

        const bool isResponseParameter = attributes
            .value(kHanwhaParameterIsResponseAttribute)
            .toString()
            .toLower() == kHanwhaTrue.toLower();


        if (!reader.readNextStartElement())
            return false;

        HanwhaCgiParameter parameter;
        parameter.setName(parameterName);
        parameter.setIsRequestParameter(isRequestParameter);
        parameter.setIsResponseParameter(isResponseParameter);

        if (!parseDataType(reader, submenu, action, parameter))
            return false;

        reader.readNextStartElement();
    }

    return true;
}

bool HanwhaCgiParameters::parseDataType(
    QXmlStreamReader& reader,
    const QString& submenu,
    const QString& action,
    HanwhaCgiParameter& parameter)
{
    if (reader.name() != kHanwhaDataTypeNodeName)
        return false;

    reader.readNextStartElement();

    if (reader.name() == kHanwhaEnumNodeName || reader.name() == kHanwhaCsvNodeName)
    {
        std::set<QString> possibleValues;
        while (reader.readNextStartElement() && reader.name() == kHanwhaEnumEntryNodeName)
        {
            auto entryValue = reader.attributes()
                .value(kHanwhaValueAttribute)
                .toString();

            if (!entryValue.isEmpty())
                possibleValues.insert(entryValue);

            reader.skipCurrentElement();
        }

        parameter.setPossibleValues(possibleValues);
        parameter.setType(HanwhaCgiParameterType::enumeration);
        m_parameters[submenu][action][parameter.name()] = parameter;
    }
    else if (reader.name() == kHanwhaIntegerNodeName)
    {
        parameter.setType(HanwhaCgiParameterType::integer);
        const auto& attributes = reader.attributes();
        bool success = false;
        
        const int min = attributes.value(kHanwhaMinValueAttribute).toInt(&success);
        if (success)
            parameter.setMin(min);

        const int max = attributes.value(kHanwhaMaxValueAttribute).toInt(&success);
        if (success)
            parameter.setMax(max);

        m_parameters[submenu][action][parameter.name()] = parameter;

        reader.readNextStartElement();
    }
    else if (reader.name() == kHanwhaBooleanNodeName)
    {
        const auto attributes = reader.attributes();
        const auto falseValue = attributes.value(kHanwhaFalseValueAttribute).toString();
        const auto trueValue = attributes.value(kHanwhaTrueValueAttribute).toString();

        parameter.setType(HanwhaCgiParameterType::boolean);
        parameter.setFalseValue(falseValue);
        parameter.setTrueValue(trueValue);
        
        m_parameters[submenu][action][parameter.name()] = parameter;

        reader.readNextStartElement();
    }
    else if (reader.name() == kHanwhaStringNodeName)
    {
        parameter.setType(HanwhaCgiParameterType::string);

        const auto attributes = reader.attributes();
        const auto formatInfo = attributes.value(kHanwhaFormatInfoAttribute).toString();
        const auto format = attributes.value(kHanwhaFormatAttribute).toString();

        parameter.setFormatInfo(formatInfo);
        parameter.setFormatString(format);

        m_parameters[submenu][action][parameter.name()] = parameter;

        reader.readNextStartElement();
    }

    reader.readNextStartElement();
    reader.readNextStartElement();

    return true;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
