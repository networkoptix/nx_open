#ifndef VIDEOWALL_ITEM_INDEX_H
#define VIDEOWALL_ITEM_INDEX_H

#include <QtCore/QMetaType>
#include <QtCore/QUuid>
#include <QtCore/QList>

#include <core/resource/resource_fwd.h>

/**
 * This class contains all the necessary information to look up a videowall item.
 */
class QnVideoWallItemIndex {
public:
    QnVideoWallItemIndex() {}

    QnVideoWallItemIndex(const QnVideoWallResourcePtr &videowall, const QUuid &uuid);

    QnVideoWallResourcePtr videowall() const;
    QnVideoWallItem item() const;
    QUuid uuid() const;

    /** \return true if the index item is not initialized. */
    bool isNull() const;

    /** \return true if the index contains valid videowall item data. */
    bool isValid() const;
private:
    QnVideoWallResourcePtr m_videowall;
    QUuid m_uuid;
};

Q_DECLARE_METATYPE(QnVideoWallItemIndex)
Q_DECLARE_METATYPE(QnVideoWallItemIndexList)


#endif // VIDEOWALL_ITEM_INDEX_H
