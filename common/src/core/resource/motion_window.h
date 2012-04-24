#ifndef __MOTION_WINDOW_H__
#define __MOTION_WINDOW_H__

#include <QRegion>

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


class QnMotionRegion
{
public:
    QnMotionRegion();

    static const int MIN_SENSITIVITY = 0; // equal motion mask
    static const int MAX_SENSITIVITY = 9; // max motion sensitivity


    bool operator==(const QnMotionRegion& other) const;
    bool operator!=(const QnMotionRegion& other) const;
    bool isEmpty() const;

    void addRect(int sensitivity, const QRect& rect);
    QRegion getMotionMask() const; // return info with zerro sensitivity only
    QRegion getRegionBySens(int value) const;
    bool updateSensitivityAt(const QPoint& pos, int sens);
private:
    QRegion m_data[MAX_SENSITIVITY - MIN_SENSITIVITY+1];
};


#endif // __MOTION_WINDOW_H__
