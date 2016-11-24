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