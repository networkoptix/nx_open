#include "videowall_item.h"

#include <QtCore/QMimeData>
#include <QtCore/QByteArray>
#include <QtCore/QDataStream>

#include <common/common_globals.h>

namespace {

const QString mimeTypeId = lit("application/x-noptix-videowall-items");

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
