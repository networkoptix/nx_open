#include "group.h"

#include <QtCore/QJsonArray>

namespace nx::vms::server::interactive_settings::components {

QQmlListProperty<Item> Group::items()
{
    return QQmlListProperty<Item>(this, m_items);
}

const QList<Item*> Group::itemsList() const
{
    return m_items;
}

void Group::setFilledCheckItems(const QVariantList& items)
{
    if (m_filledCheckItems == items)
        return;

    m_filledCheckItems = items;
    emit filledCheckItemsChanged();
}

QJsonObject Group::serializeModel() const
{
    auto result = base_type::serializeModel();

    QJsonArray items;
    for (const auto item: m_items)
        items.append(item->serializeModel());
    result[QStringLiteral("items")] = items;

    result[QStringLiteral("filledCheckItems")] = QJsonArray::fromVariantList(m_filledCheckItems);

    return result;
}

} // namespace nx::vms::server::interactive_settings::components
