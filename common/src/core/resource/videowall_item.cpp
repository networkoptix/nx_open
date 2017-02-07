#include "videowall_item.h"

#include <QtCore/QMimeData>
#include <QtCore/QByteArray>
#include <QtCore/QDataStream>

#include <common/common_globals.h>

namespace {

const QString mimeTypeId = lit("application/x-noptix-videowall-items");

} // namespace

QString QnVideoWallItem::mimeType()
{
    return mimeTypeId;
}

void QnVideoWallItem::serializeUuids(const QList<QnUuid>& uuids, QMimeData* mimeData)
{
    if (uuids.isEmpty())
        return;

    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);
    stream << uuids;

    mimeData->setData(mimeTypeId, result);
}

QList<QnUuid> QnVideoWallItem::deserializeUuids(const QMimeData* mimeData)
{
    QList<QnUuid> result;
    if (!mimeData->hasFormat(mimeTypeId))
        return result;

    QByteArray data = mimeData->data(mimeTypeId);
    QDataStream stream(&data, QIODevice::ReadOnly);
    stream >> result;

    return result;

}

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
