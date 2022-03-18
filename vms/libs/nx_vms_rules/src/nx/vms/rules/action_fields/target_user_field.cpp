// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_user_field.h"

namespace nx::vms::rules {

QVariant TargetUserField::build(const EventData& eventData) const
{
    return QVariant::fromValue(UuidSelection{
        .ids = m_ids,
        .all = m_acceptAll,
    });
}

} // namespace nx::vms::rules
