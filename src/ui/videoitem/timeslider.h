#ifndef TIMESLIDER_H
#define TIMESLIDER_H

#include <QtCore/QPropertyAnimation>
#include <QtGui/QFrame>
#include <QtGui/QSlider>
#include <QtGui/QWidget>
#include <math.h>
#include <QDebug>
#include <QPainter>
#include <QResizeEvent>

class TimeSlider : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qint64 currentValue READ currentValue WRITE setCurrentValue NOTIFY currentValueChanged)
    Q_PROPERTY(qint64 maximumValue READ maximumValue WRITE setMaximumValue NOTIFY maximumValueChanged)
    Q_PROPERTY(double scalingFactor READ scalingFactor WRITE setScalingFactor NOTIFY scalingFactorChanged)

    Q_PROPERTY(double viewPortPos READ viewPortPos WRITE setViewPortPos)
public:
//    enum Mode { TimeMode, DateMode };

    explicit TimeSlider(QWidget *parent = 0);

    qint64 currentValue() const;
    qint64 maximumValue() const; // TODO: use min and max, not length
    double scalingFactor() const;

//    Mode mode() const;
//    void setMode(Mode mode);

    QSize sizeHint() const;
    QSize minimumSizeHint() const;

    bool isMoving() { return m_slider->isSliderDown(); }
signals:
    void currentValueChanged(qint64 value);
    void maximumValueChanged(qint64 value);
    void scalingFactorChanged(qint64 value);
    void sliderPressed();
    void sliderReleased();

public slots:
    void setCurrentValue(qint64 value);
    void setMaximumValue(qint64 value);
    void setScalingFactor(double factor);

    void zoomIn();
    void zoomOut();

private:
    QSlider *m_slider;
    QFrame *m_frame;

    qint64 m_currentValue;
    qint64 m_maximumValue;
    int m_viewPortPos;

    double m_scalingFactor;
    bool m_userInput;
    int m_delta;

//    Mode m_mode;

    QPropertyAnimation *m_animation;
protected slots:
    void setViewPortPos(double value);
    void onSliderValueChanged(int value);

protected:
    double viewPortPos() const;

    qint64 delta() const;

    double fromSlider(int value);
    int toSlider(double value);
    int sliderLength() const;

    qint64 sliderRange();

    void updateSlider();

    friend class TimeLine;
};

#endif // TIMESLIDER_H
