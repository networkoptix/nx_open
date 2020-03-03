#pragma once

#include "item.h"

namespace nx::vms::server::interactive_settings::components {

/** Base class for all items which have a value. */
class ValueItem: public Item
{
    Q_OBJECT
    Q_PROPERTY(QVariant value READ value NOTIFY valueChanged)
    Q_PROPERTY(QVariant defaultValue READ defaultValue WRITE setDefaultValue
        NOTIFY defaultValueChanged)

public:
    using Item::Item;

    /**
     * @return Value that is converted to a proper type and that satisfies value constraints.
     */
    virtual QJsonValue normalizedValue(const QJsonValue& value) const { return value; }
    /**
     * @return Valid default value which should be used when the provided default value cannot be
     * normalized. This fallback value should satisfy the following assertion:
     *     value == normalizedValue(value)
     */
    virtual QJsonValue fallbackDefaultValue() const = 0;

    QJsonValue variantToJsonValue(const QVariant& value) const;

    /**
     * This method is ought to be used by Qt properties.
     * @return Value as QVariant.
     */
    QVariant value() const { return m_value.toVariant(); }
    /**
     * This method is ought to be used by Qt properties.
     * @return Set value as QVariant.
     */
    virtual void setValue(const QVariant& value);

    /**
     * @return Item value.
     */
    QJsonValue jsonValue() const { return m_value; }
    /** Set item value. */
    virtual void setJsonValue(const QJsonValue& value);

    /**
     * This method is ought to be used by Qt properties.
     * @return Default item value as QVariant.
     */
    QVariant defaultValue() const { return m_defaultValue.toVariant(); }
    /**
     * This method is ought to be used by Qt properties.
     * @return Set default item value as QVariant.
     */
    virtual void setDefaultValue(const QVariant& defaultValue);

    /**
     * @return Default item value. This value will be assigned to item value when the item is
     * created.
     */
    QJsonValue defaultJsonValue() const { return m_defaultValue; }
    /** Set default item value. See defaultItemValue(). */
    virtual void setDefaultJsonValue(const QJsonValue& defaultValue);

    /**
     * When the engine applies values, items should accept them as is without any constraints
     * checks. When the engine finishes it will ask all items to apply value constraints (via this
     * method). It is important to update values in such two-step procedure because constraints of
     * some items may rely on values of other items. Thus when engine assigns values they may
     * become temporary invalid (until other items update their values which will change
     * constraints).
     */
    virtual void applyConstraints();

    bool engineIsUpdatingValues() const;
    bool skipStringConversionWarnings() const;

    virtual QJsonObject serializeModel() const override;

    static QString serializeJsonValue(const QJsonValue& value);

signals:
    void valueChanged();
    void defaultValueChanged();

protected:
    void emitValueConvertedWarning(const QJsonValue& originalValue, const QJsonValue& value) const;
    void emitValueConversionError(const QJsonValue& originalValue, const QString& type) const;
    void emitValueConversionError(const QJsonValue& originalValue, QJsonValue::Type type) const;

protected:
    QJsonValue m_value = QJsonValue::Undefined;
    QJsonValue m_defaultValue = QJsonValue::Undefined;
};

} // namespace nx::vms::server::interactive_settings::components
