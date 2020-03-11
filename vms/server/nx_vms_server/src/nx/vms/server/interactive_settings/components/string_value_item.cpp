#include "string_value_item.h"

namespace nx::vms::server::interactive_settings::components {

QJsonValue StringValueItem::normalizedValue(const QJsonValue& value) const
{
    switch (value.type())
    {
        case QJsonValue::String:
            return value;
        case QJsonValue::Bool:
        case QJsonValue::Double:
        case QJsonValue::Null:
        {
            const auto& result = serializeJsonValue(value);
            emitValueConvertedWarning(value, result);
            return result;
        }
        default:
            emitValueConversionError(value, QJsonValue::String);
            return QJsonValue::Undefined;
    }
}

QJsonValue StringValueItem::fallbackDefaultValue() const
{
    return QString();
}

} // namespace nx::vms::server::interactive_settings::components
