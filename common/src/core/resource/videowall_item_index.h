#ifndef VIDEOWALL_ITEM_INDEX_H
#define VIDEOWALL_ITEM_INDEX_H

#include <QtCore/QMetaType>
#include <utils/common/uuid.h>
#include <QtCore/QList>

#include <core/resource/resource_fwd.h>

#include <boost/operators.hpp>

/**
 * This class contains all the necessary information to look up a videowall item.
 */
class QnVideoWallItemIndex: public boost::equality_comparable1<QnVideoWallItemIndex>  {
public:
    QnVideoWallItemIndex() {}

    QnVideoWallItemIndex(const QnVideoWallResourcePtr &videowall, const QnUuid &uuid);

    QnVideoWallResourcePtr videowall() const;
    QnVideoWallItem item() const;
    QnUuid uuid() const;

    /** \return true if the index item is not initialized. */
    bool isNull() const;

    /** \return true if the index contains valid videowall item data. */
    bool isValid() const;

    friend bool operator==(const QnVideoWallItemIndex &l, const QnVideoWallItemIndex &r) {
        return l.m_videowall == r.m_videowall && l.m_uuid == r.m_uuid;
    }
private:
    QnVideoWallResourcePtr m_videowall;
    QnUuid m_uuid;
};

Q_DECLARE_METATYPE(QnVideoWallItemIndex)
Q_DECLARE_METATYPE(QnVideoWallItemIndexList)


#endif // VIDEOWALL_ITEM_INDEX_H
