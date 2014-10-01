#ifndef VIDEOWALL_ITEM_DATA_H
#define VIDEOWALL_ITEM_DATA_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <utils/common/uuid.h>

#include <core/misc/screen_snap.h>

#include <utils/common/id.h>

class QMimeData;

/**
 * @brief The QnVideoWallItem class         Single videowall item. Can take part of the real system
 *                                          screen or even some of them. Can have a layout.
 *                                          Can be available for editing to several users at once.
 */
class QnVideoWallItem {
public:
    QnVideoWallItem(): online(false) {}

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
    bool online;


    static QString mimeType();

    static void serializeUuids(const QList<QnUuid> &uuids, QMimeData *mimeData);

    static QList<QnUuid> deserializeUuids(const QMimeData *mimeData);

    friend bool operator==(const QnVideoWallItem &l, const QnVideoWallItem &r) {
        return (l.layout == r.layout &&
                l.uuid == r.uuid &&
                l.pcUuid == r.pcUuid &&
                l.name == r.name &&
                l.online == r.online &&
                l.screenSnaps == r.screenSnaps);
    }
};

Q_DECLARE_METATYPE(QnVideoWallItem)
Q_DECLARE_TYPEINFO(QnVideoWallItem, Q_MOVABLE_TYPE);

typedef QList<QnVideoWallItem> QnVideoWallItemList;
typedef QHash<QnUuid, QnVideoWallItem> QnVideoWallItemMap;


Q_DECLARE_METATYPE(QnVideoWallItemList)
Q_DECLARE_METATYPE(QnVideoWallItemMap)

#endif // VIDEOWALL_ITEM_DATA_H
