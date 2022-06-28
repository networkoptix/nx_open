// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "email_message_field.h"

#include <nx/utils/log/assert.h>
#include <utils/email/message.h>

#include "../basic_event.h"
#include "../event_aggregator.h"

namespace nx::vms::rules {

EmailMessageField::EmailMessageField(common::SystemContext* context):
    common::SystemContextAware(context),
    m_caption(context),
    m_description(context)
{
    connect(&m_caption, &TextWithFields::textChanged, this, &EmailMessageField::captionChanged);
    connect(&m_description, &TextWithFields::textChanged, this, &EmailMessageField::descriptionChanged);
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

QVariant EmailMessageField::build(const EventAggregatorPtr& eventAggregator) const
{
    // Return correct type for testing.
    NX_ASSERT(eventAggregator->type() == "nx.events.test", "Must not be called");
    return QVariant::fromValue(nx::email::Message{});
}

} // namespace nx::vms::rules
