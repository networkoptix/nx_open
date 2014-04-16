#ifndef __MOTION_WINDOW_H__
#define __MOTION_WINDOW_H__

#include <QtCore/QMetaType>
#include <QtCore/QMultiMap>
#include <QtGui/QRegion>
#include <QtGui/QPainterPath>

/*
struct QnMotionWindow
{
    bool operator==(const QnMotionWindow& other) const;
    QnMotionWindow(): sensitivity(0) {}
    QnMotionWindow(int i, const QRegion& r): sensitivity(i), region(r) {}

    int sensitivity;
    QRegion region;
};
*/

//typedef QnMotionWindow[QnMotionWindow::MAX_SENSITIVITY-QnMotionWindow::MIN_SENSITIVITY+1] QnMotionRegion;

// TODO: #Elric rename header

class QnMotionRegion
{
public:
    enum RegionValid{VALID, WINDOWS, MASKS, SENS};

    QnMotionRegion();

    static const int MIN_SENSITIVITY = 0; // equal motion mask
    static const int DEFAULT_SENSITIVITY = 6;
    static const int MAX_SENSITIVITY = 9; // max motion sensitivity

    /** 
    * \returns WINDOWS if sum of rects in all regions in range [1..MAX] greater than maxRectCount
    * \returns MASKS if sum of rects in motionMask region (index 0) greater than maxMaskRects OR
    * \returns SENS number of regions with at least 1 rect is greater than maxMotionSens
    * \returns VALID otherwise
     */
    RegionValid isValid(int maxMotionRects, int maxMaskRects, int maxMotionSens) const;

    bool operator==(const QnMotionRegion& other) const;
    bool operator!=(const QnMotionRegion& other) const;
    bool isEmpty() const;

    void addRect(int sensitivity, const QRect& rect);
    QRegion getMotionMask() const; // return info with zero sensitivity only
    QRegion getRegionBySens(int value) const;
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
    QRegion m_data[MAX_SENSITIVITY - MIN_SENSITIVITY + 1];
    QPainterPath m_pathCache[MAX_SENSITIVITY - MIN_SENSITIVITY + 1];
    bool m_dirty;
};

Q_DECLARE_METATYPE(QnMotionRegion);


#endif // __MOTION_WINDOW_H__
