// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "settings_response.h"

namespace nx {
namespace sdk {

SettingsResponse::SettingsResponse(Ptr<StringMap> values, Ptr<StringMap> errors):
    m_values(values),
    m_errors(errors)
{
}

IStringMap* SettingsResponse::getValues() const
{
    if (m_values)
        m_values->addRef();

    return m_values.get();
}

IStringMap* SettingsResponse::getErrors() const
{
    if (m_errors)
        m_errors->addRef();

    return m_errors.get();
}

void SettingsResponse::setValue(std::string key, std::string value)
{
    if (!m_values)
        m_values = makePtr<StringMap>();

    m_values->setItem(std::move(key), std::move(value));
}

void SettingsResponse::setError(std::string key, std::string value)
{
    if (!m_errors)
        m_errors = makePtr<StringMap>();

    m_errors->setItem(std::move(key), std::move(value));
}

void SettingsResponse::setValues(Ptr<StringMap> values)
{
    m_values = values;
}

void SettingsResponse::setErrors(Ptr<StringMap> errors)
{
    m_errors = errors;
}

} // namespace sdk
} // namespace nx
