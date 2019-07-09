#include "settings_response.h"

namespace nx {
namespace sdk {

SettingsResponse::SettingsResponse(Ptr<StringMap> values, Ptr<StringMap> errors):
    m_values(values),
    m_errors(errors)
{
}

IStringMap* SettingsResponse::values() const
{
    if (m_values)
        m_values->addRef();

    return m_values.get();
}

IStringMap* SettingsResponse::errors() const
{
    if (m_errors)
        m_errors->addRef();

    return m_errors.get();
}

void SettingsResponse::setValue(std::string key, std::string value)
{
    if (!m_values)
        m_values = makePtr<StringMap>();

    m_values->addItem(std::move(key), std::move(value));
}

void SettingsResponse::setError(std::string key, std::string value)
{
    if (!m_errors)
        m_errors = makePtr<StringMap>();

    m_errors->addItem(std::move(key), std::move(value));
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
