#include "enumeration_item.h"

#include <QtCore/QJsonArray>

namespace nx::vms::server::interactive_settings::components {

void EnumerationItem::setValue(const QVariant& value)
{
    base_type::setValue(m_range.contains(value) ? value : defaultValue());
}

void EnumerationItem::setRange(const QVariantList& range)
{
    if (m_range == range)
        return;

    m_range = range;
    emit rangeChanged();

    if (!m_range.contains(value()))
        setValue(defaultValue());
}

void EnumerationItem::setItemCaptions(const QVariantMap& value)
{
    if (m_itemCaptions == value)
        return;

    m_itemCaptions = value;
    emit itemCaptionsChanged();
}

QJsonObject EnumerationItem::serialize() const
{
    auto result = base_type::serialize();
    result[QLatin1String("range")] = QJsonArray::fromVariantList(m_range);
    result[QLatin1String("itemCaptions")] = QJsonObject::fromVariantMap(m_itemCaptions);
    return result;
}

} // namespace nx::vms::server::interactive_settings::components
