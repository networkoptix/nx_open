#ifndef NAVIGATIONITEM_H
#define NAVIGATIONITEM_H

#include "unmoved/unmoved_interactive_opacity_item.h"

class QTimerEvent;
class CLVideoCamera;
class TimeSlider;
class QGraphicsProxyWidget;

class MyButton;
class TimeLabel : public QLabel
{
public:
    TimeLabel(QWidget *parent = 0) : QLabel(parent) {}

    void setCurrentValue(qint64 value);
    void setMaximumValue(qint64 value);

private:
    qint64 m_currentValue;
    qint64 m_maximumValue;
};

class NavigationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationWidget(QWidget *parent = 0);

    TimeSlider *slider() const;
    TimeLabel *label() const;

    bool isPlaying() const { return m_playing; }
    void setPlaying(bool);

private slots:
    void togglePlayPause();

signals:
    void pause();
    void play();
    void rewindBackward();
    void rewindForward();
    void stepBackward();
    void stepForward();

protected:
    void wheelEvent(QWheelEvent *) {} // to fix buuug with scene

private:
    QHBoxLayout *m_layout;
    MyButton *m_backwardButton;
    MyButton *m_playButton;
    MyButton *m_stepForwardButton;
    MyButton *m_stepBackwardButton;
    MyButton *m_forwardButton;
    bool m_playing;
    QPushButton *m_pauseButton;
    TimeSlider *m_slider;
    TimeLabel *m_label;
};

class NavigationItem : public CLUnMovedInteractiveOpacityItem
{
    Q_OBJECT
public:
    explicit NavigationItem(QGraphicsItem *parent = 0);
    ~NavigationItem();

    QWidget *navigationWidget();

    void setVideoCamera(CLVideoCamera* camera);

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) {}
    QRectF boundingRect() const;

    bool mouseOver() const { return m_mouseOver; }

    static const int DEFAULT_HEIGHT = 75;

protected:
    void timerEvent(QTimerEvent* event);
    void updateSlider();

private slots:
    void onValueChanged(qint64);
    void pause();
    void play();

    void rewindBackward();
    void rewindForward();

    void stepBackward();
    void stepForward();

    void onSliderPressed();
    void onSliderReleased();
protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *);

private:
    QGraphicsProxyWidget *m_proxy;
    NavigationWidget *m_widget;
    CLVideoCamera* m_camera;
    int m_timerId;
    bool m_sliderIsmoving;
    qint64 m_currentTime;
    bool m_mouseOver;
};

#endif // NAVIGATIONITEM_H
