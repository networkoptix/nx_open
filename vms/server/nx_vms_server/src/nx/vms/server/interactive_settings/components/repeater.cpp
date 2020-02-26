#include "repeater.h"

namespace nx::vms::server::interactive_settings::components {

Repeater::Repeater(QObject* parent):
    base_type(QStringLiteral("Repeater"), parent)
{
}

QVariant Repeater::itemTemplate() const
{
    return m_itemTemplate.toVariantMap();
}

void Repeater::setItemTemplate(const QVariant& itemTemplate)
{
    const auto templateJson = QJsonObject::fromVariantMap(itemTemplate.toMap());
    if (m_itemTemplate == templateJson)
        return;

    m_itemTemplate = templateJson;
    emit itemTemplateChanged();
}

void Repeater::setCount(int count)
{
    if (m_count == count)
        return;

    m_count = count;
    emit countChanged();
}

void Repeater::setStartIndex(int index)
{
    if (m_startIndex == index)
        return;

    m_startIndex = index;
    emit startIndexChanged();
}

void Repeater::setAddButtonCaption(const QString& caption)
{
    if (m_addButtonCaption == caption)
        return;

    m_addButtonCaption = caption;
    emit addButtonCaptionChanged();
}

QJsonObject Repeater::serialize() const
{
    auto result = base_type::serialize();
    result[QStringLiteral("addButtonCaption")] = m_addButtonCaption;
    return result;
}

} // namespace nx::vms::server::interactive_settings::components
