// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "email_message_field.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/rules/utils/openapi_doc.h>
#include <utils/email/message.h>

#include "../aggregated_event.h"
#include "../basic_event.h"

namespace nx::vms::rules {

EmailMessageField::EmailMessageField(
    common::SystemContext* context,
    const FieldDescriptor* descriptor)
    :
    ActionBuilderField{descriptor},
    common::SystemContextAware(context)
{
}

QVariant EmailMessageField::build(const AggregatedEventPtr& eventAggregator) const
{
    // Return correct type for testing.
    NX_ASSERT(eventAggregator->type().startsWith("nx.events.test"), "Must not be called");
    return QVariant::fromValue(nx::email::Message{});
}

QJsonObject EmailMessageField::openApiDescriptor(const QVariantMap&)
{
    return {};
}

} // namespace nx::vms::rules
