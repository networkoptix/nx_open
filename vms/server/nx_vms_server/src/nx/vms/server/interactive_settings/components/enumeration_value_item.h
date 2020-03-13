#pragma once

#include <QtCore/QJsonArray>

#include "string_value_item.h"

namespace nx::vms::server::interactive_settings::components {

class EnumerationValueItem: public StringValueItem
{
    Q_OBJECT
    Q_PROPERTY(QJsonValue range READ range WRITE setRange NOTIFY rangeChanged)
    Q_PROPERTY(QJsonValue itemCaptions
       READ itemCaptions WRITE setItemCaptions NOTIFY itemCaptionsChanged)

public:
    QJsonValue range() const { return QJsonArray::fromStringList(m_range); }
    void setRange(const QJsonValue& range);

    QJsonValue itemCaptions() const { return m_itemCaptions; }
    void setItemCaptions(const QJsonValue& value);

    virtual QJsonValue normalizedValue(const QJsonValue& value) const override;
    virtual QJsonValue fallbackDefaultValue() const override;
    virtual QJsonObject serializeModel() const override;

signals:
    void rangeChanged();
    void itemCaptionsChanged();

protected:
    using StringValueItem::StringValueItem;

protected:
    QStringList m_range;
    QJsonObject m_itemCaptions;
};

} // namespace nx::vms::server::interactive_settings::components
