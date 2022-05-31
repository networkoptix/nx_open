// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "email_message_field.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::rules {

EmailMessageField::EmailMessageField(common::SystemContext* context):
    common::SystemContextAware(context),
    m_caption(context),
    m_description(context)
{
}

QString EmailMessageField::caption() const
{
    return m_caption.text();
}

void EmailMessageField::setCaption(const QString& caption)
{
    m_caption.setText(caption);
}

QString EmailMessageField::description() const
{
    return m_description.text();
}

void EmailMessageField::setDescription(const QString& description)
{
    m_description.setText(description);
}

QVariant EmailMessageField::build(const EventPtr& /*event*/) const
{
    NX_ASSERT(false, "Must not be called");
    return {};
}

} // namespace nx::vms::rules
