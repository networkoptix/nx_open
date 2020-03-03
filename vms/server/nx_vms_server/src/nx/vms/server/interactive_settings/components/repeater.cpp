#include "repeater.h"

namespace nx::vms::server::interactive_settings::components {

Repeater::Repeater(QObject* parent):
    base_type(QStringLiteral("Repeater"), parent)
{
}

void Repeater::setItemTemplate(const QJsonValue& itemTemplate)
{
    if (itemTemplate.type() != QJsonValue::Object)
    {
        emitError(Issue::Code::parseError, "Repeater template must be an Object.");
        return;
    }

    const auto& object = itemTemplate.toObject();

    if (m_itemTemplate == object)
        return;

    m_itemTemplate = object;
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

QJsonObject Repeater::serializeModel() const
{
    auto result = base_type::serializeModel();
    result[QStringLiteral("addButtonCaption")] = m_addButtonCaption;
    return result;
}

} // namespace nx::vms::server::interactive_settings::components
