#include "value_item.h"

namespace nx::vms::server::interactive_settings::components {

QVariant ValueItem::value() const
{
    return m_value.isValid() ? m_value : m_defaultValue;
}

void ValueItem::setValue(const QVariant& value)
{
    if (m_value == value)
        return;

    m_value = value;
    emit valueChanged();
}

void ValueItem::setDefaultValue(const QVariant& defaultValue)
{
    if (!m_value.isValid())
        emit valueChanged();

    m_defaultValue = defaultValue;
    emit defaultValueChanged();
}

QJsonObject ValueItem::serialize() const
{
    auto result = base_type::serialize();
    result[QLatin1String("defaultValue")] = QJsonValue::fromVariant(m_defaultValue);
    return result;
}

} // namespace nx::vms::server::interactive_settings::components
