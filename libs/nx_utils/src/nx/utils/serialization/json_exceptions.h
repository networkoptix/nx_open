// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/exception.h>

namespace nx::utils::serialization::json {

class InvalidParameterException: public nx::utils::Exception
{
public:
    InvalidParameterException(const std::pair<QString, QString>& failedKeyValue):
        m_param(failedKeyValue.first), m_value(failedKeyValue.second)
    {
    }

    virtual QString message() const override
    {
        if (!m_param.isEmpty())
            return NX_FMT("Invalid parameter '%1': %2", m_param, m_value);

        // TODO: This case should be handled by the different exception type.
        return NX_FMT("Invalid value type%1%2", m_value.isEmpty() ? "" : ": ", m_value);
    }

    const QString& param() const { return m_param; }
    const QString& value() const { return m_value; }

private:
    const QString m_param;
    const QString m_value;
};

class InvalidJsonException: public nx::utils::Exception
{
public:
    InvalidJsonException(const QString& message): m_message(message) {}
    virtual QString message() const override { return m_message; }

private:
    const QString m_message;
};

} // namespace nx::utils::serialization::json
