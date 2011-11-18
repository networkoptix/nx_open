#ifndef NAVIGATIONITEM_H
#define NAVIGATIONITEM_H

#include "unmoved/unmoved_interactive_opacity_item.h"
#include <recording/device_file_catalog.h> /* For QnTimePeriod. */

class QGraphicsWidget;
class QLabel;
class QTimerEvent;

class CLVideoCamera;
class ImageButton;
class SpeedSlider;
class TimeSlider;
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

    static const int DEFAULT_HEIGHT = 60;

protected:
    void timerEvent(QTimerEvent* event);
    void updateSlider();
    void updatePeriodList();

private Q_SLOTS:
    void onLiveModeChanged(bool value);
    void onValueChanged(qint64);
    void pause();
    void play();
    void togglePlayPause();

    void rewindBackward();
    void rewindForward();

    void stepBackward();
    void stepForward();

    void jumpToLive();

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
    ImageButton *m_backwardButton;
    ImageButton *m_stepBackwardButton;
    ImageButton *m_playButton;
    ImageButton *m_stepForwardButton;
    ImageButton *m_forwardButton;
    SpeedSlider *m_speedSlider;
    QGraphicsWidget *m_graphicsWidget;
    ImageButton *m_muteButton;
    VolumeSlider *m_volumeSlider;
    QLabel *m_timeLabel;

    ImageButton *m_jumpToLiveButton;

    CLVideoCamera* m_camera;
    int m_timerId;
    qint64 m_currentTime;
    bool m_playing;
    bool m_sliderIsmoving;
    bool m_mouseOver;

    bool m_active;

    QnTimePeriod m_timePeriod;

    struct RestoreInfoTextData {
        QString extraInfoText;
        QTimer timer;
    } *restoreInfoTextData;
};

#endif // NAVIGATIONITEM_H
