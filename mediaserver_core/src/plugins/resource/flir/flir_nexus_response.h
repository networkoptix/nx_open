#pragma once

#include <assert.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include <boost/optional.hpp>

#include "flir_nexus_common.h"

namespace nx {
namespace plugins {
namespace flir {
namespace nexus {

class CommandResponse
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

    CommandResponse();
    CommandResponse(const QString &serialized);

    int returnCode() const;
    QString returnString() const;
    Type responseType() const;
    bool isValid() const;

    Type fromStringToResponseType(const QString& typeStr) const;
    void deserialize(const QString& serialized);

    template<typename T>
    boost::optional<T> value(const QString& parameterName) const
    {
        static_assert(std::is_same<T,int>::value
            || std::is_same<T,double>::value
            || std::is_same<T, QString>::value
            || std::is_same<T, bool>::value,
            "Only QString, int, double and bool specializations are allowed");
    };

private:
    Type m_responseType;
    int m_returnCode;
    QString m_returnString;
    std::map<QString, QJsonValue> m_parameters;
};

template<>
boost::optional<bool> CommandResponse::value<bool>(const QString& parameterName) const;

template<>
boost::optional<QString> CommandResponse::value<QString>(const QString& parameterName) const;

template<>
boost::optional<int> CommandResponse::value<int>(const QString& parameterName) const;

template<>
boost::optional<double> CommandResponse::value<double>(const QString& parameterName) const;


} // namespace nexus
} // namespace flir
} // namespace plugins
} // namespace nx

