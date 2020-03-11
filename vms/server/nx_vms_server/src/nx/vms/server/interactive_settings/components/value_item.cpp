#include "value_item.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>

#include <nx/utils/log/assert.h>

#include "../abstract_engine.h"

namespace nx::vms::server::interactive_settings::components {

static const QString typeString(QJsonValue::Type type)
{
    switch (type)
    {
        case QJsonValue::Bool:
            return QStringLiteral("Bool");
        case QJsonValue::Double:
            return QStringLiteral("Number");
        case QJsonValue::String:
            return QStringLiteral("String");
        case QJsonValue::Array:
            return QStringLiteral("Array");
        case QJsonValue::Object:
            return QStringLiteral("Object");
        case QJsonValue::Null:
            return QStringLiteral("Null");
        default:
            return QString();
    }
}

static const QString messageValueString(const QJsonValue& value)
{
    return typeString(value.type()) + lm(" \"%1\"").arg(ValueItem::serializeJsonValue(value));
}

QJsonValue ValueItem::variantToJsonValue(const QVariant& value) const
{
    const auto& json = QJsonValue::fromVariant(value);
    if (!value.isNull() && json.isNull())
    {
        emitError(Issue::Code::cannotConvertValue, "Unsupported value type.");
        return QJsonValue::Undefined;
    }

    return json;
}

void ValueItem::setValue(const QVariant& value)
{
    const auto& json = variantToJsonValue(value);
    if (!json.isUndefined())
        setJsonValue(json);
}

void ValueItem::setJsonValue(const QJsonValue& value)
{
    auto actualValue = value;

    if (!engineIsUpdatingValues())
    {
        actualValue = normalizedValue(value);
        if (actualValue.isUndefined())
            actualValue = m_defaultValue;
    }

    NX_ASSERT(!actualValue.isUndefined());

    if (!actualValue.isUndefined() && m_value != actualValue)
    {
        m_value = actualValue;
        emit valueChanged();
    }
}

void ValueItem::setDefaultValue(const QVariant& defaultValue)
{
    const auto& json = variantToJsonValue(defaultValue);
    if (!json.isUndefined())
        setDefaultJsonValue(json);
}

void ValueItem::setDefaultJsonValue(const QJsonValue& defaultValue)
{
    auto actualValue = defaultValue;

    if (!engineIsUpdatingValues())
    {
        actualValue = normalizedValue(defaultValue);
        if (actualValue.isUndefined())
            actualValue = fallbackDefaultValue();
    }

    NX_ASSERT(!actualValue.isUndefined());

    if (!actualValue.isUndefined() && m_defaultValue != actualValue)
    {
        m_defaultValue = actualValue;
        emit defaultValueChanged();
    }
}

void ValueItem::applyConstraints()
{
    setDefaultJsonValue(m_defaultValue);
    setJsonValue(m_value);
}

bool ValueItem::engineIsUpdatingValues() const
{
    return engine() && engine()->updatingValues();
}

bool ValueItem::skipStringConversionWarnings() const
{
    return engine() && engine()->skipStringConversionWarnings();
}

QJsonObject ValueItem::serializeModel() const
{
    auto result = Item::serializeModel();
    result[QStringLiteral("defaultValue")] = m_defaultValue;
    return result;
}

QString ValueItem::serializeJsonValue(const QJsonValue& value)
{
    switch (value.type())
    {
        case QJsonValue::Bool:
            return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        case QJsonValue::Double:
            return QString::number(value.toDouble());
        case QJsonValue::String:
            return value.toString();
        case QJsonValue::Array:
            return QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact);
        case QJsonValue::Object:
            return QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
        default:
            return QString();
    }
}

void ValueItem::emitValueConvertedWarning(
    const QJsonValue& originalValue, const QJsonValue& value) const
{
    emitWarning(Issue::Code::valueConverted, lm("Converted from %1 to %2").args(
        messageValueString(originalValue), messageValueString(value)));
}

void ValueItem::emitValueConversionError(
    const QJsonValue& originalValue, const QString& type) const
{
    emitError(Issue::Code::cannotConvertValue, lm("Cannot convert from %1 to %2").args(
        messageValueString(originalValue), type));
}

void ValueItem::emitValueConversionError(
    const QJsonValue& originalValue, QJsonValue::Type type) const
{
    emitValueConversionError(originalValue, typeString(type));
}

} // namespace nx::vms::server::interactive_settings::components
