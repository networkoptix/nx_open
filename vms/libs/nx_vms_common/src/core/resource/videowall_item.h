// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>

#include <core/misc/screen_snap.h>
#include <nx/utils/uuid.h>

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
    nx::Uuid layout;

    /**
     * @brief uuid                          Id of this item instance.
     */
    nx::Uuid uuid;

    /**
     * @brief pcUuid                        Id of the real system (PC) instance.
     */
    nx::Uuid pcUuid;

    /**
     * @brief name                          Display name of the item.
     */
    QString name;

    QnScreenSnaps screenSnaps;

    /** Status of the running videowall instance bound to this item. Runtime status, should not be serialized or saved. */
    struct
    {
        bool online = false;
        nx::Uuid controlledBy;
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

NX_VMS_COMMON_API QDebug operator<<(QDebug dbg, const QnVideoWallItem& item);

typedef QList<QnVideoWallItem> QnVideoWallItemList;
typedef QHash<nx::Uuid, QnVideoWallItem> QnVideoWallItemMap;
