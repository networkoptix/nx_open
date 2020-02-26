#pragma once

#include "value_item.h"

namespace nx::vms::server::interactive_settings::components {

class EnumerationItem: public ValueItem
{
    Q_OBJECT
    Q_PROPERTY(QVariantList range READ range WRITE setRange NOTIFY rangeChanged)

    using base_type = ValueItem;

public:
    using ValueItem::ValueItem;

    virtual void setValue(const QVariant& value) override;

    QVariantList range() const
    {
        return m_range;
    }

    void setRange(const QVariantList& range);

    virtual QJsonObject serialize() const override;

signals:
    void rangeChanged();

protected:
    QVariantList m_range;
};

} // namespace nx::vms::server::interactive_settings::components
