#include "flir_nexus_response.h"

using namespace nx::plugins::flir::nexus;

Response::Response():
    m_responseType(Type::invalid)
{};

Response::Response(const QString &serialized)
{
    deserialize(serialized);
};

int Response::returnCode() const
{
    return m_returnCode;
};

QString Response::returnString() const
{
    return m_returnString;
};

Response::Type Response::responseType() const
{
    return m_responseType;
};

bool Response::isValid() const
{
    return m_responseType != Type::invalid;
};

Response::Type Response::fromStringToResponseType(const QString& typeStr) const
{
    if (typeStr == kServerWhoAmICommand)
        return Response::Type::serverWhoAmI;
    else if (typeStr == kRequestControlCommand)
        return Response::Type::serverRemoteControlRequest;
    else if (typeStr == kReleaseControlCommand)
        return Response::Type::serverRemoteControlRelease;
    else if (typeStr == kSetOutputPortStateCommand)
        return Response::Type::ioSensorOutputStateSet;

    return Response::Type::invalid;
}

void Response::deserialize(const QString& serialized)
{
    qDebug() << "=====> Deserializing flir message" << serialized;

    const QString kReturnCodeKey = lit("Return Code");
    const QString kReturnStringKey = lit("Return String");

    auto jsonDocument = QJsonDocument::fromJson(serialized.toUtf8());
    auto jsonObject = jsonDocument.object();

    auto keys = jsonObject.keys();
    if (keys.size() != 1)
    {
        qDebug () << "=======> Keys size" << keys.size();
        m_responseType = Type::invalid;
        return;
    }

    const auto kResponseTypeStr = keys.at(0);
    qDebug () << "========> First key" << kResponseTypeStr;
    m_responseType = fromStringToResponseType(kResponseTypeStr);
    if (m_responseType == Type::invalid)
        return;

    qDebug() << "=====> ReponseType" << (int)m_responseType;

    if (!jsonObject[kResponseTypeStr].isObject())
    {
        qDebug() << "===========> Not object";
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