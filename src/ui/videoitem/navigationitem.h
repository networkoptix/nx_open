#ifndef NAVIGATIONITEM_H
#define NAVIGATIONITEM_H

#include "unmoved/unmoved_interactive_opacity_item.h"

class QTimerEvent;
class CLVideoCamera;
class TimeSlider;
class QGraphicsProxyWidget;

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

    bool playing() const { return m_playing; }
    void setPlaying(bool);

private slots:
    void togglePlayPause();
    void backwardPressed();
    void forwardPressed();

signals:
    void pause();
    void play();
    void rewindBackward();
    void rewindForward();
    void stepBackward();
    void stepForward();

private:
    QHBoxLayout *m_layout;
    QPushButton *m_backwardButton;
    QPushButton *m_playButton;
    QPushButton *m_forwardButton;
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

private:
    QGraphicsProxyWidget *m_proxy;
    NavigationWidget *m_widget;
    CLVideoCamera* m_camera;
    int m_timerId;
    bool m_sliderIsmoving;
    qint64 m_currentTime;
};

#endif // NAVIGATIONITEM_H
