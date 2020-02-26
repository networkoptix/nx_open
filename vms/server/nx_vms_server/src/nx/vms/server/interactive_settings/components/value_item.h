#pragma once

#include <QtCore/QVariant>

#include "item.h"

namespace nx::vms::server::interactive_settings::components {

class ValueItem: public Item
{
    Q_OBJECT
    Q_PROPERTY(QVariant value READ value NOTIFY valueChanged)
    Q_PROPERTY(QVariant defaultValue READ defaultValue WRITE setDefaultValue
        NOTIFY defaultValueChanged)

    using base_type = Item;

public:
    using Item::Item;

    QVariant value() const;

    virtual void setValue(const QVariant& value);

    QVariant defaultValue() const
    {
        return m_defaultValue;
    }

    void setDefaultValue(const QVariant& defaultValue);

    virtual QJsonObject serialize() const override;

signals:
    void valueChanged();
    void defaultValueChanged();

protected:
    QVariant m_value;
    QVariant m_defaultValue;
};

} // namespace nx::vms::server::interactive_settings::components
