// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_item_index.h"

#include <core/resource/videowall_resource.h>

QnVideoWallItemIndex::QnVideoWallItemIndex()
{
}

QnVideoWallItemIndex::QnVideoWallItemIndex(
    const QnVideoWallResourcePtr& videowall,
    const nx::Uuid& uuid)
    :
    m_videowall(videowall),
    m_uuid(uuid)
{
}

QnVideoWallResourcePtr QnVideoWallItemIndex::videowall() const
{
    return m_videowall;
}

nx::Uuid QnVideoWallItemIndex::uuid() const
{
    return m_uuid;
}

bool QnVideoWallItemIndex::isNull() const
{
    return m_videowall.isNull() && m_uuid.isNull();
}

bool QnVideoWallItemIndex::isValid() const
{
    return !isNull() && m_videowall && m_videowall->items()->hasItem(m_uuid);
}

QnVideoWallItem QnVideoWallItemIndex::item() const
{
    if (!isValid())
        return QnVideoWallItem();

    return m_videowall->items()->getItem(m_uuid);
}

QString QnVideoWallItemIndex::toString() const
{
    return nx::format("QnVideoWallItemIndex %1 - %2").args(m_videowall, m_uuid);
}
