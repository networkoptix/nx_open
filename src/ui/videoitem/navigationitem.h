#ifndef NAVIGATIONITEM_H
#define NAVIGATIONITEM_H

#include "unmoved/unmoved_interactive_opacity_item.h"

class QGraphicsWidget;
class QLabel;
class QTimerEvent;

class CLVideoCamera;
class ImageButtonItem;
class SpeedSlider;
class TimeSlider;
class TimeSliderToolTipItem;
class VolumeSlider;

class NavigationItem : public CLUnMovedInteractiveOpacityItem
{
    Q_OBJECT

public:
    explicit NavigationItem(QGraphicsItem *parent = 0);
    ~NavigationItem();

    QGraphicsWidget *graphicsWidget() const { return m_graphicsWidget; }

    void setVideoCamera(CLVideoCamera* camera);

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) {}
    QRectF boundingRect() const;

    bool mouseOver() const { return m_mouseOver; }

    bool isPlaying() const { return m_playing; }
    void setPlaying(bool playing);

    bool isActive() const;
    void setActive(bool active);

    static const int DEFAULT_HEIGHT = 50;

protected:
    void timerEvent(QTimerEvent* event);
    void updateSliderToolTip(qint64 time);
    void updateSlider();

private slots:
    void onValueChanged(qint64);
    void pause();
    void play();
    void togglePlayPause();

    void rewindBackward();
    void rewindForward();

    void stepBackward();
    void stepForward();

    void onSliderPressed();
    void onSliderReleased();

    void onSpeedChanged(float);

    void onVolumeLevelChanged(int);
    void restoreInfoText();

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *);
    void wheelEvent(QGraphicsSceneWheelEvent *) {} // ### hack to avoid scene move up and down

private:
    TimeSlider *m_timeSlider;
    ImageButtonItem *m_backwardButton;
    ImageButtonItem *m_stepBackwardButton;
    ImageButtonItem *m_playButton;
    ImageButtonItem *m_stepForwardButton;
    ImageButtonItem *m_forwardButton;
    SpeedSlider *m_speedSlider;
    QGraphicsWidget *m_graphicsWidget;
    ImageButtonItem *m_muteButton;
    VolumeSlider *m_volumeSlider;
    QLabel *m_timeLabel;

    CLVideoCamera* m_camera;
    int m_timerId;
    qint64 m_currentTime;
    bool m_playing;
    bool m_sliderIsmoving;
    bool m_mouseOver;

    TimeSliderToolTipItem *m_timeSliderToolTip;
    bool m_active;

    struct RestoreInfoTextData {
        QString extraInfoText;
        QTimer timer;
    } *restoreInfoTextData;
};

#endif // NAVIGATIONITEM_H
