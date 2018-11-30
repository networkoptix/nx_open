#include "hanwha_cgi_parameters.h"
#include "hanwha_common.h"
#include "hanwha_utils.h"

namespace nx {
namespace vms::server {
namespace plugins {

HanwhaCgiParameters::HanwhaCgiParameters(nx::network::http::StatusCode::Value statusCode):
    m_statusCode(statusCode)
{

}

HanwhaCgiParameters::HanwhaCgiParameters(
    const nx::Buffer& rawBuffer,
    nx::network::http::StatusCode::Value statusCode)
    :
    m_statusCode(statusCode)

{
    QXmlStreamReader reader(rawBuffer);
    m_parameters.clear();
    parseXml(reader);
    m_isValid = !reader.hasError() ||
        reader.error() == QXmlStreamReader::PrematureEndOfDocumentError;
}

boost::optional<HanwhaCgiParameter> HanwhaCgiParameters::parameter(
    const QString& cgi,
    const QString& submenu,
    const QString& action,
    const QString& parameter) const
{
    auto cgiItr = m_parameters.find(cgi);
    if (cgiItr == m_parameters.cend())
        return boost::none;

    auto submenuItr = cgiItr->second.find(submenu);
    if (submenuItr == cgiItr->second.cend())
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
    if (split.size() != 4)
    {
        NX_ASSERT(false);
        return boost::none;
    }

    return parameter(split[0], split[1], split[2], split[3]);
}

bool HanwhaCgiParameters::isValid() const
{
    return m_isValid
        && nx::network::http::StatusCode::isSuccessCode(m_statusCode)
        && !m_parameters.empty();
}

nx::network::http::StatusCode::Value HanwhaCgiParameters::statusCode() const
{
    return m_statusCode;
}

QString HanwhaCgiParameters::strAttribute(QXmlStreamReader& reader, const QString& name)
{
    return reader.attributes().value(name).toString();
}

void HanwhaCgiParameters::parseXml(
    QXmlStreamReader& reader,
    const QString& cgi,
    const QString& submenu,
    const QString& action)
{
    while (!reader.atEnd() && reader.readNextStartElement())
    {
        if (reader.name() == "cgi")
            parseXml(reader, strAttribute(reader, "name"));
        else if (reader.name() == "submenu")
            parseXml(reader, cgi, strAttribute(reader, "name"));
        else if (reader.name() == "action")
            parseXml(reader, cgi, submenu, strAttribute(reader, "name").replace(L'/', L'_'));
        else if (reader.name() == "parameter")
            parseParameter(reader, cgi, submenu, action, strAttribute(reader, "name"));
        else
            parseXml(reader);
    }
}

void HanwhaCgiParameters::parseParameter(
    QXmlStreamReader& reader,
    const QString& cgi,
    const QString& submenu,
    const QString& action,
    const QString& parameterName)
{
    const auto attributes = reader.attributes();
    const bool isRequestParameter = strAttribute(reader, kHanwhaParameterIsRequestAttribute)
        .compare(kHanwhaTrue, Qt::CaseInsensitive) == 0;

    const bool isResponseParameter = strAttribute(reader, kHanwhaParameterIsResponseAttribute)
        .compare(kHanwhaTrue, Qt::CaseInsensitive) == 0;

    HanwhaCgiParameter parameter;
    parameter.setName(parameterName);
    parameter.setIsRequestParameter(isRequestParameter);
    parameter.setIsResponseParameter(isResponseParameter);

    parseDataType(reader, &parameter);
    m_parameters[cgi][submenu][action][parameterName] = parameter;
}

void HanwhaCgiParameters::parseDataType(
    QXmlStreamReader& reader,
    HanwhaCgiParameter* outParameter)
{
    while (!reader.atEnd() && reader.readNextStartElement())
    {
        const auto& attributes = reader.attributes();

        if (reader.name() == kHanwhaDataTypeNodeName)
        {
            outParameter->setType(HanwhaCgiParameterType::enumeration);
        }
        else if (reader.name() == kHanwhaEnumEntryNodeName)
        {
            auto entryValue = strAttribute(reader, kHanwhaValueAttribute);
            if (!entryValue.isEmpty())
                outParameter->addPossibleValues(entryValue);
        }
        else if (reader.name() == kHanwhaIntegerNodeName)
        {
            outParameter->setType(HanwhaCgiParameterType::integer);
            bool success = false;

            const int min = attributes.value(kHanwhaMinValueAttribute).toInt(&success);
            if (success)
                outParameter->setMin(min);

            const int max = attributes.value(kHanwhaMaxValueAttribute).toInt(&success);
            if (success)
                outParameter->setMax(max);
        }
        else if (reader.name() == kHanwhaFloatNodeName)
        {
            outParameter->setType(HanwhaCgiParameterType::floating);
            bool success = false;

            const float min = attributes.value(kHanwhaMinValueAttribute).toFloat(&success);
            if (success)
                outParameter->setFloatMin(min);

            const float max = attributes.value(kHanwhaMaxValueAttribute).toFloat(&success);
            if (success)
                outParameter->setFloatMax(max);
        }
        else if (reader.name() == kHanwhaBooleanNodeName)
        {
            outParameter->setType(HanwhaCgiParameterType::boolean);

            outParameter->setFalseValue(strAttribute(reader, kHanwhaFalseValueAttribute));
            outParameter->setTrueValue(strAttribute(reader, kHanwhaTrueValueAttribute));
        }
        else if (reader.name() == kHanwhaStringNodeName)
        {
            outParameter->setType(HanwhaCgiParameterType::string);

            outParameter->setFormatInfo(strAttribute(reader, kHanwhaFormatInfoAttribute));
            outParameter->setFormatString(strAttribute(reader, kHanwhaFormatAttribute));
            outParameter->setMaxLength(attributes.value(kHanwhaMaxLengthAttribute).toInt());
        }

        parseDataType(reader, outParameter);
    }
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
