#ifndef NAVIGATIONITEM_H
#define NAVIGATIONITEM_H

#include <QtCore/QTimer>

#include "unmoved/unmoved_interactive_opacity_item.h"
#include <recording/device_file_catalog.h> /* For QnTimePeriod. */
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


class QGraphicsWidget;
class QLabel;
class QTimerEvent;

class QnVideoServerConnection;

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
    void addReserveCamera(CLVideoCamera* camera);
    void removeReserveCamera(CLVideoCamera* camera);

    CLVideoCamera *videoCamera() const { return m_camera; }

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) {}
    QRectF boundingRect() const;

    inline bool mouseOver() const { return m_mouseOver; }

    inline bool isPlaying() const { return m_playing; }

    static const int DEFAULT_HEIGHT = 60;

public Q_SLOTS:
    void setPlaying(bool playing);

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
    ImageButton *m_liveButton;
    SpeedSlider *m_speedSlider;
    QGraphicsWidget *m_graphicsWidget;
    ImageButton *m_muteButton;
    VolumeSlider *m_volumeSlider;
    QLabel *m_timeLabel;

    CLVideoCamera *m_camera;
    CLVideoCamera *m_forcedCamera;
    QSet<CLVideoCamera *> m_reserveCameras;
    int m_timerId;
    qint64 m_currentTime;
    bool m_playing;
    bool m_mouseOver;

    QnTimePeriod m_timePeriod;

    detail::QnTimePeriodUpdater *m_timePeriodUpdater;

    struct RestoreInfoTextData {
        QString extraInfoText;
        QTimer timer;
    } *restoreInfoTextData;
};

#endif // NAVIGATIONITEM_H
