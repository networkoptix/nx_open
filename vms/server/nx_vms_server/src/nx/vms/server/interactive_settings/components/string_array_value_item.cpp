#include "string_array_value_item.h"

#include <QtCore/QJsonDocument>

namespace nx::vms::server::interactive_settings::components {

void StringArrayValueItem::applyConstraints()
{
    // Skip possible implementation of EnumerationValueItem which operates on strings, not array.
    ValueItem::applyConstraints();
}

QJsonValue StringArrayValueItem::normalizedValue(const QJsonValue& value) const
{
    switch (value.type())
    {
        case QJsonValue::String:
        {
            const auto& str = value.toString();
            if (str.startsWith(L'[') && str.endsWith(L']'))
            {
                QJsonParseError error;
                const auto& json = QJsonDocument::fromJson(str.toUtf8(), &error);
                if (error.error != QJsonParseError::NoError)
                {
                    emitError(Issue::Code::cannotConvertValue,
                        lm("Cannot parse JSON string \"%1\": %2").args(str, error.errorString()));
                    return QJsonValue::Undefined;
                }

                if (!json.isArray())
                {
                    emitValueConversionError(value, QJsonValue::Array);
                    return QJsonValue::Undefined;
                }

                if (!skipStringConversionWarnings())
                    emitValueConvertedWarning(value, json.array());

                return normalizedValue(json.array());
            }

            const auto& normalized = EnumerationValueItem::normalizedValue(value);
            if (normalized.isUndefined())
                return normalized;

            QJsonArray result{normalized};
            emitValueConvertedWarning(value, result);

            return result;
        }
        case QJsonValue::Array:
        {
            QJsonArray result;

            for (const auto item: value.toArray())
            {
                const auto& normalized = EnumerationValueItem::normalizedValue(item);
                if (!normalized.isUndefined())
                    result.append(normalized);
            }

            return result;
        }
        case QJsonValue::Null:
        {
            emitValueConvertedWarning(value, QJsonArray());
            return QJsonArray();
        }
        default:
            emitValueConversionError(value, "Array of Strings");
            return QJsonValue::Undefined;
    }
}

QJsonValue StringArrayValueItem::fallbackDefaultValue() const
{
    return QJsonArray();
}

} // namespace nx::vms::server::interactive_settings::components
