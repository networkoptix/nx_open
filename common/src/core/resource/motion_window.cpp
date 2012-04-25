#include "motion_window.h"
#include "core/datapacket/mediadatapacket.h"

QnMotionRegion::QnMotionRegion()
{
    m_maxRects = INT_MAX;
    //addRect((QnMotionRegion::MAX_SENSITIVITY - QnMotionRegion::MIN_SENSITIVITY)/2, QRect(0,0,MD_WIDTH, MD_HEIGHT));
    addRect(0, QRect(0,0,MD_WIDTH, MD_HEIGHT));
}

QRegion QnMotionRegion::getMotionMask() const
{
    return m_data[0];
}

QRegion QnMotionRegion::getRegionBySens(int value) const
{
    return m_data[value];
}

QMap<int, QRect> QnMotionRegion::getAllMotionRects() const
{
    QMap<int, QRect> result;
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
        bool foundUnion = false;
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
    if (sensitivity != 0) {
        for (int i = MIN_SENSITIVITY; i <= MAX_SENSITIVITY; ++i)
        {
            m_data[i] -= rect;
        }
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

void QnMotionRegion::setMaxRectCount(int value)
{
    m_maxRects = value;
}

bool QnMotionRegion::isValid() const
{
    for (int sens = QnMotionRegion::MIN_SENSITIVITY+1; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens)
    {
        if (getRectsBySens(sens).size() > m_maxRects)
            return false;
    }
    return true;
}

int QnMotionRegion::getCurrentRectCount() const
{
    int rez = 0;
    for (int sens = QnMotionRegion::MIN_SENSITIVITY+1; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens)
        rez = qMax(rez, getRectsBySens(sens).size());
    return rez;
}

int QnMotionRegion::getMaxRectCount() const
{
    return m_maxRects;
}
