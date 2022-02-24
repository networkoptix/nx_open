// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_item.h"

#include <QtCore/QDebug>

namespace {

const QString mimeTypeId = "application/x-noptix-videowall-items";

} // namespace

QDebug operator<<(QDebug dbg, const QnVideoWallItem& item)
{
    dbg.nospace()
        << "QnVideoWallItem(" << item.name
        << "[" << item.uuid.toSimpleString() << "]";
    if (!item.layout.isNull())
        dbg.nospace() << " layout[" << item.layout.toSimpleString() << "]";
    dbg.nospace()
        << " pc[" << item.pcUuid.toSimpleString() << "]"
        << " at " << item.screenSnaps
        << ")";
    return dbg.space();
}
