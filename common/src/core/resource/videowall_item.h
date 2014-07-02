#ifndef VIDEOWALL_ITEM_DATA_H
#define VIDEOWALL_ITEM_DATA_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QUuid>

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
    QnId layout;

    /**
     * @brief uuid                          Id of this item instance.
     */
    QUuid uuid;

    /**
     * @brief pcUuid                        Id of the real system (PC) instance.
     */
    QUuid pcUuid;

    /**
     * @brief name                          Display name of the item.
     */
    QString name;

    /** Status of the running videowall instance bound to this item. Runtime status, should not be serialized or saved. */
    bool online;

    /**
     * @brief geometry                      Position and size of the item in the Virtual Desktop
     *                                      coordinate system.
     */
    QRect geometry;

    static QString mimeType();

    static void serializeUuids(const QList<QUuid> &uuids, QMimeData *mimeData);

    static QList<QUuid> deserializeUuids(const QMimeData *mimeData);

    friend bool operator==(const QnVideoWallItem &l, const QnVideoWallItem &r) {
        return (l.layout == r.layout &&
                l.uuid == r.uuid &&
                l.pcUuid == r.pcUuid &&
                l.name == r.name &&
                l.online == r.online &&
                l.geometry == r.geometry);
    }
};

Q_DECLARE_METATYPE(QnVideoWallItem)
Q_DECLARE_TYPEINFO(QnVideoWallItem, Q_MOVABLE_TYPE);

typedef QList<QnVideoWallItem> QnVideoWallItemList;
typedef QHash<QUuid, QnVideoWallItem> QnVideoWallItemMap;


Q_DECLARE_METATYPE(QnVideoWallItemList)
Q_DECLARE_METATYPE(QnVideoWallItemMap)

#endif // VIDEOWALL_ITEM_DATA_H
