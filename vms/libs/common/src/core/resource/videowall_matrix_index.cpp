#include "videowall_matrix_index.h"

#include <core/resource/videowall_resource.h>

QnVideoWallMatrixIndex::QnVideoWallMatrixIndex()
{
}

QnVideoWallMatrixIndex::QnVideoWallMatrixIndex(
    const QnVideoWallResourcePtr& videowall,
    const QnUuid& uuid)
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

QnUuid QnVideoWallMatrixIndex::uuid() const
{
    return m_uuid;
}

void QnVideoWallMatrixIndex::setUuid(const QnUuid& uuid)
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
    return lm("QnVideoWallMatrixIndex %1 - %2").args(m_videowall, m_uuid);
}
