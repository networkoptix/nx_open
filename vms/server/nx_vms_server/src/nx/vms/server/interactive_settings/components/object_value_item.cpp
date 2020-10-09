#include "object_value_item.h"

#include <QtCore/QJsonDocument>

namespace nx::vms::server::interactive_settings::components {

namespace {

QJsonObject mergeObjects(const QJsonObject& object, const QJsonObject& newObject)
{
    QJsonObject result = object;

    // Empty object is considered a special value and turns corresponding object in the old
    // structure to an empty one without actual merging. Total replacement in the case of an empty
    // object is needed to allow plugins to report ROI item deletion.
    if (newObject.isEmpty())
        return QJsonObject();

    for (auto it = newObject.begin(); it != newObject.end(); ++it)
    {
        const QString& key = it.key();
        QJsonValueRef value = result[key];

        if (it->isObject() && value.isObject())
            value = mergeObjects(value.toObject(), it->toObject());
        else
            value = it.value();
    }

    return result;
}

} // namespace

QJsonValue ObjectValueItem::normalizedValue(const QJsonValue& value) const
{
    switch (value.type())
    {
        case QJsonValue::Null:
        case QJsonValue::Object:
            return value;
        case QJsonValue::String:
        {
            QJsonParseError error;
            const auto& json = QJsonDocument::fromJson(value.toString().toUtf8(), &error);
            if (error.error != QJsonParseError::NoError)
            {
                emitError(Issue::Code::cannotConvertValue,
                    lm("Cannot parse JSON string \"%1\": %2").args(
                        value.toString(), error.errorString()));
                return QJsonValue::Undefined;
            }

            if (!json.isObject())
            {
                emitValueConversionError(value, QJsonValue::Object);
                return QJsonValue::Undefined;
            }

            if (!skipStringConversionWarnings())
                emitValueConvertedWarning(value, json.object());
            return json.object();
        }
        default:
            emitValueConversionError(value, QJsonValue::Object);
            return QJsonValue::Undefined;
    }
}

QJsonValue ObjectValueItem::fallbackDefaultValue() const
{
    return QJsonObject();
}

void ObjectValueItem::setJsonValue(const QJsonValue& value)
{
    const auto& normalized = normalizedValue(value);
    if (normalized.isUndefined())
        return;

    if (m_value.isObject() && normalized.isObject())
    {
        const auto& result = mergeObjects(m_value.toObject(), normalized.toObject());
        ValueItem::setJsonValue(result);
        return;
    }

    ValueItem::setJsonValue(normalized);
}

} // namespace nx::vms::server::interactive_settings::components
