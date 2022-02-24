// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "show_notification_action.h"

namespace nx::vms::rules {

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