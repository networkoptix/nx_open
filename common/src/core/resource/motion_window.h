#ifndef __MOTION_WINDOW_H__
#define __MOTION_WINDOW_H__

#include <QtCore/QMetaType>
#include <QtCore/QMultiMap>
#include <QtCore/QMutex>
#include <QtGui/QRegion>
#include <QtGui/QPainterPath>


/*!
    Copy-constructor and assignment operator perform deep-copy of object 
    Problem with QRegion is that cannot be used with object's shallow copies safely in multiple threads simultaneously 
    because \a QRegion::rects() const method is not thread-safe (due to call to \a QRegionPrivate::vectorize function)
*/
class QnRegion
:
    public QRegion
{
public:
    QnRegion();
    //!makes deep copy of QRegion
    QnRegion(const QnRegion& right);
    QnRegion(int x, int y, int w, int h, RegionType t = Rectangle);
    QnRegion(const QPolygon & a, Qt::FillRule fillRule = Qt::OddEvenFill);
    QnRegion(const QRegion & r);
    QnRegion(const QBitmap & bm);
    QnRegion(const QRect & r, RegionType t = Rectangle);

    QnRegion& operator=( const QnRegion& r );

    QVector<QRect> rects() const;

private:
    mutable QMutex m_mutex;
};

// TODO: #Elric rename header

class QnMotionRegion
{
public:
    // TODO: #Elric #enum btw, associated API is also totally evil
    enum class ErrorCode {
        Ok,
        Windows,
        Masks,
        Sens
    };

    QnMotionRegion();
    QnMotionRegion( const QnMotionRegion& );

    QnMotionRegion& operator=( const QnMotionRegion& );

    static const int MIN_SENSITIVITY = 0; // equal motion mask
    static const int DEFAULT_SENSITIVITY = 5;
    static const int MAX_SENSITIVITY = 9; // max motion sensitivity

    /** 
    * \returns Windows if sum of rects in all regions in range [1..MAX] greater than maxRectCount
    * \returns Masks if sum of rects in motionMask region (index 0) greater than maxMaskRects OR
    * \returns Sens number of regions with at least 1 rect is greater than maxMotionSens
    * \returns Ok otherwise
     */
    ErrorCode isValid(int maxMotionRects, int maxMaskRects, int maxMotionSens) const;

    bool operator==(const QnMotionRegion& other) const;
    bool operator!=(const QnMotionRegion& other) const;
    bool isEmpty() const;

    void addRect(int sensitivity, const QRect& rect);
    QnRegion getMotionMask() const; // return info with zero sensitivity only
    QnRegion getRegionBySens(int value) const;
    QMultiMap<int, QRect> getAllMotionRects() const;

    /**
     * Returns painter path for the motion mask region.
     */
    QPainterPath getMotionMaskPath();

    /**
     * Returns painter path for the given sensitivity region.
     * \param value                 Target sensitivity level.
     */
    QPainterPath getRegionBySensPath(int value);

    /**
     * Returns simplified version of region's rects
     */
    QVector<QRect> getRectsBySens(int value) const;

    bool updateSensitivityAt(const QPoint& pos, int sens);

    int getMotionRectCount() const;
    int getMaskRectCount() const;
    int getMotionSensCount() const;

    void removeDefaultMotion();
private:
    void updatePathCache();
private:
    QnRegion m_data[MAX_SENSITIVITY - MIN_SENSITIVITY + 1];
    QPainterPath m_pathCache[MAX_SENSITIVITY - MIN_SENSITIVITY + 1];
    bool m_dirty;
};

Q_DECLARE_METATYPE(QnMotionRegion);

void parseMotionRegion(QnMotionRegion& region, const QByteArray& regionString);
QString serializeMotionRegion(const QnMotionRegion& region);

void parseMotionRegionList(QList<QnMotionRegion>& regions, const QByteArray& regionsString);
QString serializeMotionRegionList(const QList<QnMotionRegion>& regions);


#endif // __MOTION_WINDOW_H__
