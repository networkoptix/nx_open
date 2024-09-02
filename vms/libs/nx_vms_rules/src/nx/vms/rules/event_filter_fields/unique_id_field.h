// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../event_filter_field.h"

namespace nx::vms::rules {

/** Generates unique id once and matches it with an event, e.g. soft trigger. Invisible. */
class NX_VMS_RULES_API UniqueIdField: public EventFilterField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.uniqueId")

    Q_PROPERTY(nx::Uuid id READ id WRITE setId)

public:
    using EventFilterField::EventFilterField;

    nx::Uuid id() const;
    void setId(nx::Uuid id);

    bool match(const QVariant& eventValue) const override;
    static QJsonObject openApiDescriptor();

private:
    mutable nx::Uuid m_id;
};

} // namespace nx::vms::rules
