// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_id_field.h"

#include <QtCore/QDataStream>

#include "../aggregated_event.h"
#include "../utils/field.h"

namespace nx::vms::rules {

QVariant EventIdField::build(const AggregatedEventPtr& event) const
{
    auto eventResourceId = event->property(utils::kCameraIdFieldName).value<QnUuid>();
    if (eventResourceId.isNull())
        eventResourceId = event->property(utils::kServerIdFieldName).value<QnUuid>();

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << (qint64) event->timestamp().count() << eventResourceId;

    return QVariant::fromValue(QnUuid::fromArbitraryData(data));
}

} // namespace nx::vms::rules
