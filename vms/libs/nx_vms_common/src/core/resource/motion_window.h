// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef __MOTION_WINDOW_H__
#define __MOTION_WINDOW_H__

#include <optional>

#include <QtCore/QMultiMap>
#include <QtGui/QPainterPath>
#include <QtGui/QRegion>

#include <nx/utils/thread/mutex.h>

/*!
    Copy-constructor and assignment operator perform deep-copy of object
    Problem with QRegion is that cannot be used with object's shallow copies safely in multiple threads simultaneously
    because \a QRegion::rects() const method is not thread-safe (due to call to \a QRegionPrivate::vectorize function)
*/
class NX_VMS_COMMON_API QnRegion
:
    public QRegion
{
public:
    QnRegion();
    //!makes deep copy of QRegion
    QnRegion(const QnRegion& right);
    QnRegion(int x, int y, int w, int h, RegionType t = Rectangle);
    QnRegion(const QPolygon & a, Qt::FillRule fillRule = Qt::OddEvenFill);
    QnRegion(const QBitmap & bm);
    QnRegion(const QRect & r, RegionType t = Rectangle);

    QnRegion& operator=( const QnRegion& r );

    QVector<QRect> rects() const;

private:
    mutable nx::Mutex m_mutex;
};

class NX_VMS_COMMON_API QnMotionRegion
{
public:
    enum class ErrorCode {
        Ok,
        Windows,
        Masks,
        Sens
    };

    QnMotionRegion();
    QnMotionRegion( const QnMotionRegion& );

    QnMotionRegion& operator=( const QnMotionRegion& );

    static constexpr int kDefaultSensitivity = 5;
    static constexpr int kSensitivityLevelCount = 10;

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

    /**
     * Region where motion must be skipped (zero sensitivity).
     */
    QnRegion getMotionMask() const;

    QnRegion getRegionBySens(int value) const;
    QMultiMap<int, QRect> getAllMotionRects() const;

    /**
     * Returns painter path for the motion mask region.
     */
    QPainterPath getMotionMaskPath() const;

    /**
     * Returns painter path for the given sensitivity region.
     * \param value                 Target sensitivity level.
     */
    QPainterPath getRegionBySensPath(int value) const;

    /**
     * Returns simplified version of region's rects
     */
    QVector<QRect> getRectsBySens(int value) const;

    bool updateSensitivityAt(const QPoint& pos, int sens);

    int getMotionRectCount() const;
    int getMaskRectCount() const;
    int getMotionSensCount() const;

    void removeDefaultMotion();

    QString toString() const;
private:
    void updatePathCache() const;
private:
    QnRegion m_data[kSensitivityLevelCount];
    mutable QPainterPath m_pathCache[kSensitivityLevelCount];
    mutable bool m_dirty = false;
};

NX_VMS_COMMON_API std::optional<QnMotionRegion> parseMotionRegion(const QByteArray& regionString);

QString serializeMotionRegion(const QnMotionRegion& region);

NX_VMS_COMMON_API std::optional<QList<QnMotionRegion>> parseMotionRegionList(
    const QByteArray& regionsString);

QString serializeMotionRegionList(const QList<QnMotionRegion>& regions);


#endif // __MOTION_WINDOW_H__
