// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../event_filter_field.h"

namespace nx::vms::rules {

/**
 * Should be used for event properties that are not used for filtering but are required in
 * the event manifest.
 */
class NX_VMS_RULES_API DummyField: public EventFilterField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.dummy")

public:
    using EventFilterField::EventFilterField;

    bool match(const QVariant& value) const override;
};

} // namespace nx::vms::rules
