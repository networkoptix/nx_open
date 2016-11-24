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
        static_assert(false, "Only QString, int, double and bool specializations are allowed");
    };
    
    template<>
    boost::optional<bool> value<bool>(const QString& parameterName) const
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
    boost::optional<QString> value<QString>(const QString& parameterName) const
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
    boost::optional<int> value<int>(const QString& parameterName) const
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
    boost::optional<double> value<double>(const QString& parameterName) const
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

private:
    Type m_responseType;
    int m_returnCode;
    QString m_returnString;
    std::map<QString, QJsonValue> m_parameters;
};


} // namespace nexus
} // namespace flir
} // namespace plugins
} // namespace nx

