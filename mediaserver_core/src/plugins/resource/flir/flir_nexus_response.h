#pragma once

#include <type_traits>
#include <assert.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include "flir_nexus_common.h"

namespace nx{
namespace plugins{
namespace flir{
namespace nexus{

class Response
{
public:
    enum class Type
    {
        invalid,
        serverWhoAmI,
        serverRemoteControlRequest,
        serverRemoteControlRelease,
        ioSensorOutputStateSet
    };

    Response();
    Response(const QString &serialized);

    int returnCode() const;
    QString returnString() const;
    Type responseType() const;
    bool isValid() const;

    Type fromStringToResponseType(const QString& typeStr) const;
    void deserialize(const QString& serialized);

    template<typename T>
    T value(const QString& parameterName) const;
    
    template<>
    bool value<bool>(const QString& parameterName) const
    {
        NX_ASSERT(m_parameters.at(parameterName).isBool(), lm("Json value is not of boolean type."));
        return m_parameters.at(parameterName).toBool();
    }

    template<>
    QString value<QString>(const QString& parameterName) const
    {
        NX_ASSERT(m_parameters.at(parameterName).isString(), lm("Json value is not of string type."));
        return m_parameters.at(parameterName).toString();
    }

    template<>
    int value<int>(const QString& parameterName) const
    {
        NX_ASSERT(m_parameters.at(parameterName).isDouble(), lm("Json value is not of double type."));
        return m_parameters.at(parameterName).toInt();
    }

    template<>
    double value<double>(const QString& parameterName) const
    {
        NX_ASSERT(m_parameters.at(parameterName).isDouble(), lm("Json value is not of double type."));
        return m_parameters.at(parameterName).toDouble();
    }

private:
    Type m_responseType;
    int m_returnCode;
    QString m_returnString;
    std::map<QString, QJsonValue> m_parameters;
};


}// namespace nexus
}// namespace flir
}// namespace plugins
}// namespace nx

