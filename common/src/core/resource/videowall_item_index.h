#ifndef VIDEOWALL_ITEM_INDEX_H
#define VIDEOWALL_ITEM_INDEX_H

#include <QtCore/QMetaType>
#include <QtCore/QUuid>
#include <QtCore/QList>

#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_resource.h>

/**
 * This class contains all the necessary information to look up a layout item.
 */
class QnVideoWallItemIndex {
public:
    QnVideoWallItemIndex() {}

    QnVideoWallItemIndex(const QnVideoWallResourcePtr &videowall, const QUuid &uuid):
        m_videowall(videowall), m_uuid(uuid)
    {}

    const QnVideoWallResourcePtr &videowall() const {
        return m_videowall;
    }

    void setVideoWall(const QnVideoWallResourcePtr &videowall) {
        m_videowall = videowall;
    }

    const QUuid &uuid() const {
        return m_uuid;
    }

    void setUuid(const QUuid &uuid) {
        m_uuid = uuid;
    }

    bool isNull() const {
        return m_videowall.isNull() || m_uuid.isNull();
    }

private:
    QnVideoWallResourcePtr m_videowall;
    QUuid m_uuid;
};

Q_DECLARE_METATYPE(QnVideoWallItemIndex)
Q_DECLARE_METATYPE(QnVideoWallItemIndexList)


#endif // VIDEOWALL_ITEM_INDEX_H
