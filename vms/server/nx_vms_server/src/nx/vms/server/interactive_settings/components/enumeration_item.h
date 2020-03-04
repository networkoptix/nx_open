#pragma once

#include "value_item.h"

namespace nx::vms::server::interactive_settings::components {

class EnumerationItem: public ValueItem
{
    Q_OBJECT
    Q_PROPERTY(QVariantList range READ range WRITE setRange NOTIFY rangeChanged)
    Q_PROPERTY(QVariantMap itemCaptions
        READ itemCaptions WRITE setItemCaptions NOTIFY itemCaptionsChanged)

    using base_type = ValueItem;

public:
    using ValueItem::ValueItem;

    virtual void setValue(const QVariant& value) override;

    QVariantList range() const { return m_range; }
    void setRange(const QVariantList& range);

    QVariantMap itemCaptions() const { return m_itemCaptions; }
    void setItemCaptions(const QVariantMap& value);

    virtual QJsonObject serialize() const override;

signals:
    void rangeChanged();
    void itemCaptionsChanged();

protected:
    QVariantList m_range;
    QVariantMap m_itemCaptions;
};

} // namespace nx::vms::server::interactive_settings::components
