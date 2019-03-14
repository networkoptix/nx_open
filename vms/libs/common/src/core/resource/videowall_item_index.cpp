#include "videowall_item_index.h"

#include <core/resource/videowall_resource.h>

QnVideoWallItemIndex::QnVideoWallItemIndex(const QnVideoWallResourcePtr &videowall, const QnUuid &uuid) :
    m_videowall(videowall), m_uuid(uuid)
{
}

QnVideoWallResourcePtr QnVideoWallItemIndex::videowall() const {
    return m_videowall;
}

QnUuid QnVideoWallItemIndex::uuid() const {
    return m_uuid;
}

bool QnVideoWallItemIndex::isNull() const {
    return m_videowall.isNull() && m_uuid.isNull();
}

bool QnVideoWallItemIndex::isValid() const {
    return !isNull() && m_videowall->items()->hasItem(m_uuid);
}

QnVideoWallItem QnVideoWallItemIndex::item() const {
    if (!isValid())
        return QnVideoWallItem();
    return m_videowall->items()->getItem(m_uuid);
}
