#include "numeric_value_items.h"

#include <cmath>

namespace nx::vms::server::interactive_settings::components {

QJsonValue NumericValueItem::normalizedValue(const QJsonValue& value) const
{
    switch (value.type())
    {
        case QJsonValue::Double:
            return value;
        case QJsonValue::String:
        {
            bool ok = false;
            const auto result = value.toString().toDouble(&ok);
            if (ok)
            {
                if (!skipStringConversionWarnings())
                    emitValueConvertedWarning(value, result);
                return result;
            }

            emitValueConversionError(value, QJsonValue::Double);
            return m_defaultValue;
        }
        case QJsonValue::Null:
            emitValueConvertedWarning(value, false);
            return 0.0;
        default:
            emitValueConversionError(value, QJsonValue::Double);
            return QJsonValue::Undefined;
    }
}

QJsonValue NumericValueItem::fallbackDefaultValue() const
{
    return 0.0;
}

void IntegerValueItem::setMinValue(int minValue)
{
    if (m_minValue == minValue)
        return;

    m_minValue = minValue;
    emit minValueChanged();

    if (!engineIsUpdatingValues())
        applyConstraints();
}

void IntegerValueItem::setMaxValue(int maxValue)
{
    if (m_maxValue == maxValue)
        return;

    m_maxValue = maxValue;
    emit maxValueChanged();

    if (!engineIsUpdatingValues())
        applyConstraints();
}

QJsonValue IntegerValueItem::normalizedValue(const QJsonValue& value) const
{
    auto normalized = NumericValueItem::normalizedValue(value);
    if (normalized.isUndefined())
        return normalized;

    const auto val = normalized.toDouble();
    const auto rounded = std::round(val);
    if (!qFuzzyIsNull(val - rounded))
        emitValueConvertedWarning(value, rounded);

    return qBound(m_minValue, (int) rounded, m_maxValue);
}

QJsonValue IntegerValueItem::fallbackDefaultValue() const
{
    return std::max(0, m_minValue);
}

QJsonObject IntegerValueItem::serializeModel() const
{
    auto result = ValueItem::serializeModel();
    result[QStringLiteral("minValue")] = m_minValue;
    result[QStringLiteral("maxValue")] = m_maxValue;
    return result;
}

void RealValueItem::setMinValue(double minValue)
{
    if (qFuzzyCompare(m_minValue, minValue))
        return;

    m_minValue = minValue;
    emit minValueChanged();

    if (!engineIsUpdatingValues())
        applyConstraints();
}

void RealValueItem::setMaxValue(double maxValue)
{
    if (qFuzzyCompare(m_maxValue, maxValue))
        return;

    m_maxValue = maxValue;
    emit maxValueChanged();

    if (!engineIsUpdatingValues())
        applyConstraints();
}

QJsonValue RealValueItem::normalizedValue(const QJsonValue& value) const
{
    auto normalized = NumericValueItem::normalizedValue(value);
    if (normalized.isUndefined())
        return normalized;

    return qBound(m_minValue, normalized.toDouble(), m_maxValue);
}

QJsonValue RealValueItem::fallbackDefaultValue() const
{
    return std::max(0.0, m_minValue);
}

QJsonObject RealValueItem::serializeModel() const
{
    auto result = ValueItem::serializeModel();
    result[QStringLiteral("minValue")] = m_minValue;
    result[QStringLiteral("maxValue")] = m_maxValue;
    return result;
}

} // namespace nx::vms::server::interactive_settings::components
