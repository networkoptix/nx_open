#include "flir_nexus_response.h"

using namespace nx::plugins::flir::nexus;

CommandResponse::CommandResponse():
    m_responseType(Type::invalid)
{};

CommandResponse::CommandResponse(const QString &serialized)
{
    deserialize(serialized);
};

int CommandResponse::returnCode() const
{
    return m_returnCode;
};

QString CommandResponse::returnString() const
{
    return m_returnString;
};

CommandResponse::Type CommandResponse::responseType() const
{
    return m_responseType;
};

bool CommandResponse::isValid() const
{
    return m_responseType != Type::invalid;
};

CommandResponse::Type CommandResponse::fromStringToResponseType(const QString& typeStr) const
{
    if (typeStr == kServerWhoAmICommand)
        return CommandResponse::Type::serverWhoAmI;
    else if (typeStr == kRequestControlCommand)
        return CommandResponse::Type::serverRemoteControlRequest;
    else if (typeStr == kReleaseControlCommand)
        return CommandResponse::Type::serverRemoteControlRelease;
    else if (typeStr == kSetOutputPortStateCommand)
        return CommandResponse::Type::ioSensorOutputStateSet;

    return CommandResponse::Type::invalid;
}

void CommandResponse::deserialize(const QString& serialized)
{
    const QString kReturnCodeKey = lit("Return Code");
    const QString kReturnStringKey = lit("Return String");

    auto jsonDocument = QJsonDocument::fromJson(serialized.toUtf8());
    auto jsonObject = jsonDocument.object();

    auto keys = jsonObject.keys();
    if (keys.size() != 1)
    {
        m_responseType = Type::invalid;
        return;
    }

    const auto kResponseTypeStr = keys.at(0);
    m_responseType = fromStringToResponseType(kResponseTypeStr);
    if (m_responseType == Type::invalid)
        return;

    if (!jsonObject[kResponseTypeStr].isObject())
    {
        m_responseType = Type::invalid;
        return;
    }

    const auto kResponseParams = jsonObject[kResponseTypeStr].toObject();
    for (const auto& key : kResponseParams.keys())
    {
        if (key == kReturnCodeKey)
            m_returnCode = kResponseParams[key].toInt();
        else if (key == kReturnStringKey)
            m_returnString = kResponseParams[key].toString();
        else
            m_parameters[key] = kResponseParams[key];
    }
}

template<>
boost::optional<bool> CommandResponse::value<bool>(const QString& parameterName) const
{
    bool isBool = m_parameters.at(parameterName).isBool();
    NX_ASSERT(
        isBool,
        lm("Json value is not of boolean type. Requested parameter name %1")
            .arg(parameterName));

    if (isBool)
        return m_parameters.at(parameterName).toBool();

    return boost::none;
}

template<>
boost::optional<QString> CommandResponse::value<QString>(const QString& parameterName) const
{
    bool isString = m_parameters.at(parameterName).isString();
    NX_ASSERT(
        isString,
        lm("Json value is not of string type. Requested parameter name: %1")
            .arg(parameterName));

    if (isString)
        return m_parameters.at(parameterName).toString();

    return boost::none;
}

template<>
boost::optional<int> CommandResponse::value<int>(const QString& parameterName) const
{
    bool isNumeric = m_parameters.at(parameterName).isDouble();
    NX_ASSERT(
        isNumeric,
        lm("Json value is not of double type. Requested parameter name: %1")
            .arg(parameterName));

    if (isNumeric)
        return m_parameters.at(parameterName).toInt();

    return boost::none;
}

template<>
boost::optional<double> CommandResponse::value<double>(const QString& parameterName) const
{
    bool isNumeric = m_parameters.at(parameterName).isDouble();
    NX_ASSERT(
        isNumeric,
        lm("Json value is not of double type. Requested parameter name: %1")
            .arg(parameterName));

    if (isNumeric)
        return m_parameters.at(parameterName).toInt();

    return boost::none;
}
