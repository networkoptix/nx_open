// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_matrix_index.h"

#include <core/resource/videowall_resource.h>

QnVideoWallMatrixIndex::QnVideoWallMatrixIndex()
{
}

QnVideoWallMatrixIndex::QnVideoWallMatrixIndex(
    const QnVideoWallResourcePtr& videowall,
    const nx::Uuid& uuid)
    :
    m_videowall(videowall),
    m_uuid(uuid)
{
}

QnVideoWallResourcePtr QnVideoWallMatrixIndex::videowall() const
{
    return m_videowall;
}

void QnVideoWallMatrixIndex::setVideoWall(const QnVideoWallResourcePtr& videowall)
{
    m_videowall = videowall;
}

nx::Uuid QnVideoWallMatrixIndex::uuid() const
{
    return m_uuid;
}

void QnVideoWallMatrixIndex::setUuid(const nx::Uuid& uuid)
{
    m_uuid = uuid;
}

bool QnVideoWallMatrixIndex::isNull() const
{
    return m_videowall.isNull() || m_uuid.isNull();
}

bool QnVideoWallMatrixIndex::isValid() const
{
    return !isNull() && m_videowall->matrices()->hasItem(m_uuid);
}

QnVideoWallMatrix QnVideoWallMatrixIndex::matrix() const
{
    if (!isValid())
        return QnVideoWallMatrix();

    return m_videowall->matrices()->getItem(m_uuid);
}

QString QnVideoWallMatrixIndex::toString() const
{
    return nx::format("QnVideoWallMatrixIndex %1 - %2").args(m_videowall, m_uuid);
}
