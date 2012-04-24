#include "motion_window.h"
#include "core/datapacket/mediadatapacket.h"

QnMotionRegion::QnMotionRegion()
{
    addRect((QnMotionRegion::MAX_SENSITIVITY - QnMotionRegion::MIN_SENSITIVITY)/2, QRect(0,0,MD_WIDTH, MD_HEIGHT));
}

QRegion QnMotionRegion::getMotionMask() const
{
    return m_data[0];
}

QRegion QnMotionRegion::getRegionBySens(int value) const
{
    return m_data[value];
}

void QnMotionRegion::addRect(int sensitivity, const QRect& rect)
{
    if (sensitivity != 0) {
        for (int i = MIN_SENSITIVITY; i <= MAX_SENSITIVITY; ++i)
            m_data[i] -= rect;
    }
    m_data[sensitivity] += rect;
    qDebug() << m_data[sensitivity];
}

bool QnMotionRegion::operator==(const QnMotionRegion& other) const
{
    for (int sens = QnMotionRegion::MIN_SENSITIVITY; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens)
    {
        if (!(m_data[sens] == other.m_data[sens]))
            return false;
    }
    return true;
}

bool QnMotionRegion::operator!=(const QnMotionRegion& other) const
{
    for (int sens = QnMotionRegion::MIN_SENSITIVITY; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens)
    {
        if (!(m_data[sens] == other.m_data[sens]))
            return true;
    }
    return false;
}

bool QnMotionRegion::isEmpty() const
{
    for (int sens = QnMotionRegion::MIN_SENSITIVITY; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens)
    {
        if (!m_data[sens].isEmpty())
            return false;
    }
    return true;
}

bool QnMotionRegion::updateSensitivityAt(const QPoint& pos, int newSens)
{
    for (int sens = QnMotionRegion::MIN_SENSITIVITY; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens)
    {
        if (m_data[sens].contains(pos))
        {
            if (sens == newSens)
                return false;
            QVector<QRect>	rects = m_data[sens].rects();
            QRegion linkedRects;
            for (int i = 0; i < rects.size(); ++i)
            {
                if (rects[i].contains(pos)) {
                    linkedRects += rects[i];
                    rects.remove(i);
                    break;
                }
            }
            bool findMore = true;
            while(findMore)
            {
                findMore = false;
                for (int i = 0; i < rects.size(); ++i)
                {
                    if (!linkedRects.intersect(rects[i].translated(1,0)).isEmpty() ||
                        !linkedRects.intersect(rects[i].translated(0,1)).isEmpty() ||
                        !linkedRects.intersect(rects[i].translated(-1,0)).isEmpty() ||
                        !linkedRects.intersect(rects[i].translated(0,-1)).isEmpty())
                    {
                        linkedRects += rects[i];
                        rects.remove(i);
                        findMore = true;
                        break;
                    }
                }
            }
            m_data[sens] = QRegion();
            for (int i = 0; i < rects.size(); ++i)
                m_data[sens] += rects[i];
            m_data[newSens] += linkedRects;
            return true;
        }
    }
    return false;
}
