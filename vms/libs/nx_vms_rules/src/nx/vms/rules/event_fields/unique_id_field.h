// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../event_field.h"

namespace nx::vms::rules {

/** Generates unique id once and matches it with an event, e.g. soft trigger. Invisible. */
class NX_VMS_RULES_API UniqueIdField: public EventField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.uniqueId")

    Q_PROPERTY(QnUuid id READ id WRITE setId)

public:
    UniqueIdField() = default;

    QnUuid id() const;
    void setId(QnUuid id);

    bool match(const QVariant& eventValue) const override;

private:
    mutable QnUuid m_id;
};

} // namespace nx::vms::rules
