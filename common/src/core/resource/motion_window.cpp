#include "motion_window.h"
#include "core/datapacket/mediadatapacket.h"

QnMotionRegion::QnMotionRegion()
{
    addRect(8, QRect(0,0,MD_WIDTH, MD_HEIGHT));
    //addRect(0, QRect(0,0,MD_WIDTH, MD_HEIGHT));
}

void QnMotionRegion::removeDefaultMotion()
{
    for (int i = MIN_SENSITIVITY; i <= MAX_SENSITIVITY; ++i)
        m_data[i] = QRegion();
}

QRegion QnMotionRegion::getMotionMask() const
{
    return m_data[0];
}

QRegion QnMotionRegion::getRegionBySens(int value) const
{
    return m_data[value];
}

QMultiMap<int, QRect> QnMotionRegion::getAllMotionRects() const
{
    QMultiMap<int, QRect> result;
    for (int sens = MIN_SENSITIVITY+1; sens <= MAX_SENSITIVITY; ++sens)
    {
        QVector<QRect> rects = getRectsBySens(sens);
        for (int i = 0; i < rects.size(); ++i)
            result.insert(sens, rects[i]);
    }
    return result;
}


QVector<QRect> QnMotionRegion::getRectsBySens(int value) const
{
    QVector<QRect> rects = m_data[value].rects();
    QVector<int> ignoreList;
    ignoreList.resize(rects.size());
    // simplify rects
    for (int i = 0; i < rects.size(); ++i)
    {
        for (int j = 0; j < i; ++j)
        {
            if (rects[i].x() == rects[j].x() && rects[i].width() == rects[j].width() && rects[i].y() == rects[j].bottom()+1)
            {
                rects[i].setTop(rects[j].top());
                ignoreList[j] = true;
            }
        }
    }
    QVector<QRect> result;
    for (int i = 0; i < rects.size(); ++i)
    {
        if (!ignoreList[i])
            result << rects[i];
    }
    return result;
}

void QnMotionRegion::addRect(int sensitivity, const QRect& rect)
{
    for (int i = MIN_SENSITIVITY; i <= MAX_SENSITIVITY; ++i)
    {
        m_data[i] -= rect;
    }
    m_data[sensitivity] += rect;
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

QnMotionRegion::RegionValid QnMotionRegion::isValid(int maxMotionRects, int maxMaskRects, int maxMotionSens) const
{
    int count = 0;
    int sens_count = 0;
    for (int sens = QnMotionRegion::MIN_SENSITIVITY+1; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens)
    {
        int rects = getRectsBySens(sens).size();
        count +=  rects;
        if (rects > 0)
            sens_count++;
    }
    if (maxMaskRects > 0 && getRectsBySens(0).size() > maxMaskRects)
        return MASKS;

    if (maxMotionSens > 0) {
        if (sens_count > maxMotionSens)
            return SENS;
        return VALID;
    }

    /** only if maxMotionSens == 0 */
    if(count > maxMotionRects)
        return WINDOWS;
    return VALID;
}

int QnMotionRegion::getMotionRectCount() const
{
    int count = 0;
    for (int sens = QnMotionRegion::MIN_SENSITIVITY+1; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens)
    {
        count +=  getRectsBySens(sens).size();
    }
    return count;
}

int QnMotionRegion::getMaskRectCount() const
{
    return getRectsBySens(0).size();
}

int QnMotionRegion::getMotionSensCount() const
{
    int sens_count = 0;
    for (int sens = QnMotionRegion::MIN_SENSITIVITY+1; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens)
    {
        if (getRectsBySens(sens).size() > 0)
            sens_count++;
    }
    return sens_count;
}