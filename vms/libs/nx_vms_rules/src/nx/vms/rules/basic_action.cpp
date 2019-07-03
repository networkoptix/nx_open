#include "basic_action.h"

namespace nx::vms::rules {

BasicAction::BasicAction(const QString& type): m_type(type)
{
}

QString BasicAction::type() const
{
    return m_type;
}

QString NotificationAction::caption() const
{
    return m_caption;
}

void NotificationAction::setCaption(const QString& caption)
{
    m_caption = caption;
}

QString NotificationAction::description() const
{
    return m_description;
}

void NotificationAction::setDescription(const QString& description)
{
    m_description = description;
}

} // namespace nx::vms::rules