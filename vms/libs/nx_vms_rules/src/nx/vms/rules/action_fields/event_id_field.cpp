// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_id_field.h"

#include <QtCore/QDataStream>
#include <QtCore/QIODevice>

#include "../aggregated_event.h"
#include "../ini.h"
#include "../utils/field.h"

namespace nx::vms::rules {

QVariant EventIdField::build(const AggregatedEventPtr& event) const
{
    NX_ASSERT(!event->id().isNull());

    return QVariant::fromValue(event->id());
}

} // namespace nx::vms::rules
