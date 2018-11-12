#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <nx/utils/uuid.h>

#include <core/misc/screen_snap.h>

#include <utils/common/id.h>

class QMimeData;

/**
 * @brief The QnVideoWallItem class         Single videowall item. Can take part of the real system
 *                                          screen or even some of them. Can have a layout.
 *                                          Can be available for editing to several users at once.
 */
class QnVideoWallItem
{
public:
    /**
     * @brief layout                        Id of this item's layout resource (if any).
     */
    QnUuid layout;

    /**
     * @brief uuid                          Id of this item instance.
     */
    QnUuid uuid;

    /**
     * @brief pcUuid                        Id of the real system (PC) instance.
     */
    QnUuid pcUuid;

    /**
     * @brief name                          Display name of the item.
     */
    QString name;

    QnScreenSnaps screenSnaps;

    /** Status of the running videowall instance bound to this item. Runtime status, should not be serialized or saved. */
    struct
    {
        bool online = false;
        QnUuid controlledBy;
    } runtimeStatus;

    friend bool operator==(const QnVideoWallItem& l, const QnVideoWallItem& r)
    {
        return (l.layout == r.layout
            && l.uuid == r.uuid
            && l.pcUuid == r.pcUuid
            && l.name == r.name
            && l.screenSnaps == r.screenSnaps
            && l.runtimeStatus.online == r.runtimeStatus.online
            && l.runtimeStatus.controlledBy == r.runtimeStatus.controlledBy
            );
    }
};

QDebug operator<<(QDebug dbg, const QnVideoWallItem& item);

Q_DECLARE_METATYPE(QnVideoWallItem)
Q_DECLARE_TYPEINFO(QnVideoWallItem, Q_MOVABLE_TYPE);

typedef QList<QnVideoWallItem> QnVideoWallItemList;
typedef QHash<QnUuid, QnVideoWallItem> QnVideoWallItemMap;

Q_DECLARE_METATYPE(QnVideoWallItemList)
Q_DECLARE_METATYPE(QnVideoWallItemMap)
