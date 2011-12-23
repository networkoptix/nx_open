#ifndef NAVIGATIONITEM_H
#define NAVIGATIONITEM_H

#include <QtCore/QTimer>

#include <QtGui/QGraphicsWidget>

#include "recording/time_period.h"
#include <api/VideoServerConnection.h>

namespace detail {
    class QnTimePeriodUpdater: public QObject {
        Q_OBJECT;
    public:
        QnTimePeriodUpdater(QObject *parent = NULL): QObject(parent), m_updatePending(false), m_updateNeeded(false) {}

        void update(const QnVideoServerConnectionPtr &connection, const QnNetworkResourceList &networkResources, const QnTimePeriod &timePeriod);

    signals:
        void ready(const QnTimePeriodList &timePeriods);

    private slots:
        void at_replyReceived(const QnTimePeriodList &timePeriods);

    private:
        void sendRequest();

    private:
        QnVideoServerConnectionPtr m_connection;
        QnNetworkResourceList m_networkResources;
        QnTimePeriod m_timePeriod;
        bool m_updatePending;
        bool m_updateNeeded;
    };

}


class QLabel;
class QTimerEvent;

class QnVideoServerConnection;

class CLVideoCamera;
class ImageButton;
class SpeedSlider;
class TimeSlider;
class VolumeSlider;
class GraphicsLabel;

#define EMULATE_CLUnMovedInteractiveOpacityItem

class NavigationItem : public QGraphicsWidget
{
    Q_OBJECT

public:
    explicit NavigationItem(QGraphicsItem *parent = 0);
    ~NavigationItem();

    void setVideoCamera(CLVideoCamera* camera);
    void addReserveCamera(CLVideoCamera* camera);
    void removeReserveCamera(CLVideoCamera* camera);

    CLVideoCamera *videoCamera() const { return m_camera; }

    inline bool isPlaying() const { return m_playing; }

    static const int DEFAULT_HEIGHT = 60;

#ifdef EMULATE_CLUnMovedInteractiveOpacityItem
    // isUnderMouse() replacement;
    // qt bug 18797 When setting the flag ItemIgnoresTransformations for an item, it will receive mouse events as if it was transformed by the view.
    inline bool isMouseOver() const { return m_underMouse; }

    inline void hideIfNeeded(int duration) { hide(duration); }
    void setVisibleAnimated(bool visible, int duration);
    inline void hide(int duration) { setVisibleAnimated(false, duration); }
    inline void show(int duration) { setVisibleAnimated(true, duration); }
    void changeOpacity(qreal new_opacity, int duration = 0);

public Q_SLOTS:
    void stopAnimation();
#endif

public Q_SLOTS:
    void setPlaying(bool playing);

Q_SIGNALS:
    void exportRange(qint64 begin, qint64 end);

protected:
    void timerEvent(QTimerEvent* event);
    void updateSlider();
    void updatePeriodList(bool force);

    void smartSeek(qint64 timeMSec);
    void setActualCamera(CLVideoCamera *camera);
    void updateActualCamera();

    void updateTimePeriods(const QnVideoServerConnection *connection, const QnNetworkResourceList &resources);

private Q_SLOTS:
    void onLiveModeChanged(bool value);
    void pause();
    void play();
    void togglePlayPause();

    void rewindBackward();
    void rewindForward();

    void stepBackward();
    void stepForward();

    void onSliderPressed();
    void onSliderReleased();
    void onValueChanged(qint64);

    void onSpeedChanged(float);

    void onVolumeLevelChanged(int);

    void restoreInfoText();

    void onTimePeriodUpdaterReady(const QnTimePeriodList &timePeriods);

protected:
#ifdef EMULATE_CLUnMovedInteractiveOpacityItem
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
#endif
    void wheelEvent(QGraphicsSceneWheelEvent *) {} // ### hack to avoid scene move up and down

private:
    TimeSlider *m_timeSlider;
    ImageButton *m_backwardButton;
    ImageButton *m_stepBackwardButton;
    ImageButton *m_playButton;
    ImageButton *m_stepForwardButton;
    ImageButton *m_forwardButton;
    ImageButton *m_liveButton;
    SpeedSlider *m_speedSlider;
    ImageButton *m_muteButton;
    VolumeSlider *m_volumeSlider;
    GraphicsLabel *m_timeLabel;

    CLVideoCamera *m_camera;
    CLVideoCamera *m_forcedCamera;
    QSet<CLVideoCamera *> m_reserveCameras;
    int m_timerId;
    qint64 m_currentTime;
    bool m_playing;

    QnTimePeriod m_timePeriod;

    detail::QnTimePeriodUpdater *m_timePeriodUpdater;

    struct RestoreInfoTextData {
        QString extraInfoText;
        QTimer timer;
    } *restoreInfoTextData;

#ifdef EMULATE_CLUnMovedInteractiveOpacityItem
    bool m_underMouse;

    QPropertyAnimation *m_animation;

    static qreal m_normal_opacity;//= 0.5;
    static qreal m_active_opacity;//= 0.95;
#endif
};

#endif // NAVIGATIONITEM_H
