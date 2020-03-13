#include "enumeration_value_item.h"

#include <QtCore/QJsonArray>

namespace nx::vms::server::interactive_settings::components {

void EnumerationValueItem::setRange(const QJsonValue& range)
{
    if (range.type() != QJsonValue::Array)
    {
        emitValueConversionError(range, QJsonValue::Array);
        return;
    }

    QStringList fixedRange;
    for (const auto value: range.toArray())
    {
        const auto& normalized = StringValueItem::normalizedValue(value);
        if (!normalized.isUndefined())
            fixedRange.append(normalized.toString());
    }

    if (m_range == fixedRange)
        return;

    m_range = fixedRange;
    emit rangeChanged();

    if (m_defaultValue.isNull())
        setDefaultJsonValue(fallbackDefaultValue());

    if (!engineIsUpdatingValues())
        applyConstraints();
}

void EnumerationValueItem::setItemCaptions(const QJsonValue& value)
{
    if (value.type() != QJsonValue::Object && value.type() != QJsonValue::Null)
        emitError(Issue::Code::cannotConvertValue, "itemCaptions must be an object.");

    const auto object = value.toObject();
    if (m_itemCaptions == object)
        return;

    m_itemCaptions = object;
    emit itemCaptionsChanged();
}

QJsonValue EnumerationValueItem::normalizedValue(const QJsonValue& value) const
{
    const auto& normalized = StringValueItem::normalizedValue(value);
    if (normalized.isUndefined())
        return normalized;

    if (m_range.contains(normalized.toString()))
        return normalized;

    if (m_range.isEmpty() && normalized.toString().isEmpty())
        return normalized;

    emitError(Issue::Code::cannotConvertValue, lm("String value \"%1\" is not in range [%2]")
        .args(normalized.toString(), m_range.join(L',')));

    return QJsonValue::Undefined;
}

QJsonValue EnumerationValueItem::fallbackDefaultValue() const
{
    return m_range.isEmpty() ? QString() : m_range.first();
}

QJsonObject EnumerationValueItem::serializeModel() const
{
    auto result = StringValueItem::serializeModel();
    result[QStringLiteral("range")] = QJsonArray::fromStringList(m_range);
    result[QStringLiteral("itemCaptions")] = m_itemCaptions;
    return result;
}

} // namespace nx::vms::server::interactive_settings::components
