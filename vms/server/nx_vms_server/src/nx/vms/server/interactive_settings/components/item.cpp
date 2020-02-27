#include "item.h"

namespace nx::vms::server::interactive_settings::components {

Item::Item(const QString& type, QObject* parent):
    QObject(parent),
    m_type(type)
{
}

QJsonObject Item::serialize() const
{
    QJsonObject result;
    result[QLatin1String("type")] = m_type;
    if (!m_name.isEmpty())
        result[QLatin1String("name")] = m_name;
    if (!m_caption.isEmpty())
        result[QLatin1String("caption")] = m_caption;
    if (!m_description.isEmpty())
        result[QLatin1String("description")] = m_description;
    return result;
}

} // namespace nx::vms::server::interactive_settings::components
