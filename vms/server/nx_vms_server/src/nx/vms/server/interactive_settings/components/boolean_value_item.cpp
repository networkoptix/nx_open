#include "boolean_value_item.h"

namespace nx::vms::server::interactive_settings::components {

QJsonValue BooleanValueItem::normalizedValue(const QJsonValue& value) const
{
    switch (value.type())
    {
        case QJsonValue::Bool:
            return value;
        case QJsonValue::String:
        {
            QJsonValue result = QJsonValue::Undefined;

            const auto& str = value.toString();
            if (str == "true" || str == "1")
                result = true;
            else if (str == "false" || str == "0" || str.isEmpty())
                result = false;
            else
                emitValueConversionError(value, QJsonValue::Bool);

            if (!result.isUndefined() && !skipStringConversionWarnings())
                emitValueConvertedWarning(value, result);

            return result;
        }
        case QJsonValue::Double:
        {
            const auto& val = value.toDouble();
            if (val == 0.0 || val == 1.0)
            {
                bool result = (val == 1.0);
                emitValueConvertedWarning(value, result);
                return result;
            }

            emitValueConversionError(value, QJsonValue::Bool);
            return QJsonValue::Undefined;
        }
        case QJsonValue::Null:
            emitValueConvertedWarning(value, false);
            return false;
        default:
            emitValueConversionError(value, QJsonValue::Bool);
            return QJsonValue::Undefined;
    }
}

QJsonValue BooleanValueItem::fallbackDefaultValue() const
{
    return false;
}

} // namespace nx::vms::server::interactive_settings::components
