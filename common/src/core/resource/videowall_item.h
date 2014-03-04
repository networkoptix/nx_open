#ifndef VIDEOWALL_ITEM_DATA_H
#define VIDEOWALL_ITEM_DATA_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QUuid>

#include <utils/common/id.h>

/**
 * @brief The QnVideoWallItem class         Single videowall item. Can take part of the real system
 *                                          screen or even some of them. Can have a layout.
 *                                          Can be available for editing to several users at once.
 */
class QnVideoWallItem {
public:
    QnVideoWallItem() {}

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

    /**
     * @brief geometry                      Position and size of the item in the Virtual Desktop
     *                                      coordinate system.
     */
    QRect geometry;

    static QString mimeType() {
        return QLatin1String("application/x-noptix-videowall-items");
    }

    friend bool operator==(const QnVideoWallItem &l, const QnVideoWallItem &r) {
        return (l.layout == r.layout &&
                l.uuid == r.uuid &&
                l.pcUuid == r.pcUuid &&
                l.name == r.name &&
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
