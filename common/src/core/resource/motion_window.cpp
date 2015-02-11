
#include "motion_window.h"

#include <utils/common/mutex.h>

#include "core/datapacket/media_data_packet.h"



////////////////////////////////////////////////////////////
//// QnRegion class
////////////////////////////////////////////////////////////

QnRegion::QnRegion()
{
}

QnRegion::QnRegion(const QnRegion& right)
:
    QRegion()
{
    //making deep copy of right
    const QVector<QRect>& _rects = right.rects();
    setRects( _rects.constData(), _rects.size() );
}

QnRegion::QnRegion(int x, int y, int w, int h, RegionType t)
:
    QRegion( x, y, w, h, t )
{
}

QnRegion::QnRegion(const QPolygon & a, Qt::FillRule fillRule)
:
    QRegion( a, fillRule )
{
}

QnRegion::QnRegion(const QBitmap & bm)
:
    QRegion( bm )
{
}

QnRegion::QnRegion(const QRect & r, RegionType t)
:
    QRegion( r, t )
{
}

QnRegion& QnRegion::operator=( const QnRegion& r )
{
    if( this != &r )
    {
        //making deep copy of right
        const QVector<QRect>& _rects = r.rects();
        setRects( _rects.constData(), _rects.size() );
    }
    return *this;
}

QVector<QRect> QnRegion::rects() const
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );
    return QRegion::rects();
}



////////////////////////////////////////////////////////////
//// QnMotionRegion class
////////////////////////////////////////////////////////////

QnMotionRegion::QnMotionRegion()
{
    m_dirty = false;
    addRect(DEFAULT_SENSITIVITY, QRect(0,0,MD_WIDTH, MD_HEIGHT));
}

QnMotionRegion::QnMotionRegion( const QnMotionRegion& right )
:
    m_dirty( right.m_dirty )
{
    for( size_t i = 0; i < sizeof(m_data) / sizeof(*m_data); ++i )
        m_data[i] = right.m_data[i];
    for( size_t i = 0; i < sizeof(m_pathCache) / sizeof(*m_pathCache); ++i )
        m_pathCache[i] = right.m_pathCache[i];
}

QnMotionRegion& QnMotionRegion::operator=( const QnMotionRegion& right )
{
    for( size_t i = 0; i < sizeof(m_data) / sizeof(*m_data); ++i )
        m_data[i] = right.m_data[i];
    for( size_t i = 0; i < sizeof(m_pathCache) / sizeof(*m_pathCache); ++i )
        m_pathCache[i] = right.m_pathCache[i];
    m_dirty = right.m_dirty;
    return *this;
}

void QnMotionRegion::removeDefaultMotion()
{
    for (int i = MIN_SENSITIVITY; i <= MAX_SENSITIVITY; ++i)
        m_data[i] = QnRegion();
     m_dirty = true;
}

QnRegion QnMotionRegion::getMotionMask() const
{
    return m_data[0];
}

QnRegion QnMotionRegion::getRegionBySens(int value) const
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

QPainterPath QnMotionRegion::getMotionMaskPath() {
    if (m_dirty)
        updatePathCache();
    return m_pathCache[0];
}

QPainterPath QnMotionRegion::getRegionBySensPath(int value) {
    if (m_dirty)
        updatePathCache();
    return m_pathCache[value];
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
    m_dirty = true;
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
            QVector<QRect> rects = m_data[sens].rects();
            QnRegion linkedRects;
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
                    if (!linkedRects.intersected(rects[i].translated(1,0)).isEmpty() ||
                        !linkedRects.intersected(rects[i].translated(0,1)).isEmpty() ||
                        !linkedRects.intersected(rects[i].translated(-1,0)).isEmpty() ||
                        !linkedRects.intersected(rects[i].translated(0,-1)).isEmpty())
                    {
                        linkedRects += rects[i];
                        rects.remove(i);
                        findMore = true;
                        break;
                    }
                }
            }
            m_data[sens] = QnRegion();
            for (int i = 0; i < rects.size(); ++i)
                m_data[sens] += rects[i];
            m_data[newSens] += linkedRects;
            m_dirty = true;
            return true;
        }
    }
    return false;
}

QnMotionRegion::ErrorCode QnMotionRegion::isValid(int maxMotionRects, int maxMaskRects, int maxMotionSens) const
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
        return ErrorCode::Masks;

    if (maxMotionSens > 0) {
        if (sens_count > maxMotionSens)
            return ErrorCode::Sens;
        return ErrorCode::Ok;
    }

    /** only if maxMotionSens == 0 */
    if(count > maxMotionRects)
        return ErrorCode::Windows;
    return ErrorCode::Ok;
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

void QnMotionRegion::updatePathCache()
{
    for (int sens = QnMotionRegion::MIN_SENSITIVITY; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens){
        QnRegion region = m_data[sens];
        if (sens > 0)
            region -= m_data[0];
        QPainterPath path;
        path.addRegion(region);
        m_pathCache[sens] = path.simplified();
    }
    m_dirty = false;
}

/************************************************************************/
/* Serialization functions                                              */
/************************************************************************/

void parseMotionRegionList(QList<QnMotionRegion>& regions, const QByteArray& regionsString)
{
    QList<QByteArray> regList = regionsString.split(':');
    regions.clear();
    // for compatibility with previous version. By default screen filled medium sensitivity motion window
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        regions << QnMotionRegion();

    for (int i = 0; i < regList.size(); ++i)
        parseMotionRegion(regions[i], regList[i]);
}

void parseMotionRegion(QnMotionRegion& region, const QByteArray& regionString)
{
    QList<QRect> motionMask;
    bool firstRect = true;
    for (const QByteArray& rectString: regionString.split(';'))
    {
        QList<QByteArray> rectList = rectString.split(',');
        QRect r;
        int sensitivity = 0;
        if (rectList.size() == 4)
        {
            // for compatibility with version 1.0. Version 1.1 uses 5 parameters (sensitivity is added)
            r.setLeft(rectList[0].toInt());
            r.setTop(rectList[1].toInt());
            r.setWidth(rectList[2].toInt());
            r.setHeight(rectList[3].toInt());
        }
        else if (rectList.size() == 5)
        {
            if (firstRect) {
                region.removeDefaultMotion();
                firstRect = false;
            }
            sensitivity = rectList[0].toInt();
            r.setLeft(rectList[1].toInt());
            r.setTop(rectList[2].toInt());
            r.setWidth(rectList[3].toInt());
            r.setHeight(rectList[4].toInt());
        }
        if (sensitivity > 0)
            region.addRect(sensitivity, r);
        else
            motionMask << r;
    }
    for (int i = 0; i < motionMask.size(); ++i)
        region.addRect(0, motionMask[i]);
}

QString serializeMotionRegion(const QnMotionRegion& region)
{
    QStringList regionList;

    //for (const QnMotionWindow& window: region)
    for (int i = QnMotionRegion::MIN_SENSITIVITY; i <= QnMotionRegion::MAX_SENSITIVITY; ++i)
    {
        if (!region.getRegionBySens(i).isEmpty())
        {
            for(const QRect& rect: region.getRectsBySens(i))
            {
                QStringList rectList;
                rectList << QString::number(i) << QString::number(rect.left()) << QString::number(rect.top()) << QString::number(rect.width()) << QString::number(rect.height());
                regionList << rectList.join(QLatin1String(","));
            }
        }            
    }

    return regionList.join(QLatin1String(";"));
}

QString serializeMotionRegionList(const QList<QnMotionRegion>& regions)
{
    QString result;
    for (int i = 0; i < regions.size(); ++i)
    {
        QString regStr = serializeMotionRegion(regions[i]);
        if (i > 0)
            result += QLatin1Char(':');
        result += regStr;
    }
    return result;
}

