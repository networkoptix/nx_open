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
#include <QProxyStyle>

class TimeLine;

class SliderProxyStyle : public QProxyStyle
{
public:
    SliderProxyStyle(): QProxyStyle()
    {
        setBaseStyle(qApp->style());
    }

    int styleHint(StyleHint hint, const QStyleOption *option = 0, const QWidget *widget = 0, QStyleHintReturn *returnData = 0) const
    {
        if (hint == QStyle::SH_Slider_AbsoluteSetButtons)
            return Qt::LeftButton;
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }

    static SliderProxyStyle *instance();
};

class MySlider : public QSlider
{
public:
    MySlider(QWidget *parent = 0) : QSlider(parent) {}

    void paintEvent(QPaintEvent *ev);
    //    QSize minimumSizeHint() { return QSize(/*QSlider::sizeHint().width()*/200, 16); }
    //    QSize sizeHint() { return QSize(QSlider::sizeHint().width(), 16); }
};

class TimeSlider : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qint64 currentValue READ currentValue WRITE setCurrentValue NOTIFY currentValueChanged)
    Q_PROPERTY(qint64 maximumValue READ maximumValue WRITE setMaximumValue NOTIFY maximumValueChanged)
    Q_PROPERTY(double scalingFactor READ scalingFactor WRITE setScalingFactor NOTIFY scalingFactorChanged)
    Q_PROPERTY(double minOpacity READ minOpacity WRITE setMinOpacity)

    Q_PROPERTY(qint64 viewPortPos READ viewPortPos WRITE setViewPortPos)
public:
    //    enum Mode { TimeMode, DateMode };

    explicit TimeSlider(QWidget *parent = 0);
    ~TimeSlider() {}

    qint64 currentValue() const;
    qint64 maximumValue() const; // TODO: use min and max, not length
    double scalingFactor() const;
    double minOpacity() const;

//    Mode mode() const;
//    void setMode(Mode mode);

    QSize sizeHint() const;
    QSize minimumSizeHint() const;

    bool isMoving() { return m_slider->isSliderDown(); }
    void setMoving(bool b) { m_slider->setSliderDown(b); }

    bool centralise() const { return m_centralise; }
    void setCentralise(bool b) { m_centralise = b; }

    int sliderValue() const { return m_slider->value(); }
    int sliderLength() const;
    qint64 viewPortPos() const;
    qint64 sliderRange();

    qint64 minimumRange() const;
    void setMinimumRange(qint64);

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
    void setMinOpacity(double value);

    void zoomIn();
    void zoomOut();

private slots:
    void onWheelAnimationFinished();
private:
    MySlider *m_slider;
    TimeLine *m_frame;

    qint64 m_currentValue;
    qint64 m_maximumValue;
    qint64 m_viewPortPos;

    double m_scalingFactor;
    bool m_isUserInput;
    bool m_sliderPressed;
    int m_delta;

    //    Mode m_mode;

    QPropertyAnimation *m_animation;
    bool m_centralise;
    qint64 m_minimumRange;

protected slots:
    void setViewPortPos(qint64 value);
    void onSliderValueChanged(int value);
    void onSliderReleased();
    void onSliderPressed();
protected:

    double delta() const;

    double fromSlider(int value);
    int toSlider(double value);

    void updateSlider();
    void centraliseSlider();
    virtual bool eventFilter(QObject *target, QEvent *event);

    friend class TimeLine;
};

#endif // TIMESLIDER_H
