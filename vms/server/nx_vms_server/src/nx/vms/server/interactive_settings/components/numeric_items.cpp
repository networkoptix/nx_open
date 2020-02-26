#include "numeric_items.h"

namespace nx::vms::server::interactive_settings::components {

void IntegerNumberItem::setValue(int value)
{
    ValueItem::setValue(qBound(m_minValue, value, m_maxValue));
}

void IntegerNumberItem::setValue(const QVariant& value)
{
    if (value.canConvert(QVariant::Int))
        setValue(value.toInt());
}

void IntegerNumberItem::setMinValue(int minValue)
{
    if (m_minValue == minValue)
        return;

    m_minValue = minValue;
    emit minValueChanged();

    if (m_value.isValid())
        setValue(value());
}

void IntegerNumberItem::setMaxValue(int maxValue)
{
    if (m_maxValue == maxValue)
        return;

    m_maxValue = maxValue;
    emit maxValueChanged();

    if (m_value.isValid())
        setValue(value());
}

QJsonObject IntegerNumberItem::serialize() const
{
    auto result = base_type::serialize();
    result[QLatin1String("minValue")] = m_minValue;
    result[QLatin1String("maxValue")] = m_maxValue;
    return result;
}

void RealNumberItem::setValue(double value)
{
    ValueItem::setValue(qBound(m_minValue, value, m_maxValue));
}

void RealNumberItem::setValue(const QVariant& value)
{
    if (value.canConvert(QVariant::Double))
        setValue(value.toDouble());
}

void RealNumberItem::setMinValue(double minValue)
{
    if (qFuzzyCompare(m_minValue, minValue))
        return;

    m_minValue = minValue;
    emit minValueChanged();

    if (m_value.isValid())
        setValue(value());
}

void RealNumberItem::setMaxValue(double maxValue)
{
    if (qFuzzyCompare(m_maxValue, maxValue))
        return;

    m_maxValue = maxValue;
    emit maxValueChanged();

    if (m_value.isValid())
        setValue(value());
}

QJsonObject RealNumberItem::serialize() const
{
    auto result = base_type::serialize();
    result[QLatin1String("minValue")] = m_minValue;
    result[QLatin1String("maxValue")] = m_maxValue;
    return result;
}

} // namespace nx::vms::server::interactive_settings::components
