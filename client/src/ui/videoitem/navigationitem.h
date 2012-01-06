#ifndef NAVIGATIONITEM_H
#define NAVIGATIONITEM_H

#include <QtCore/QTimer>

#include <QtGui/QGraphicsWidget>

#include "recording/time_period.h"
#include "camera/time_period_reader_helper.h"


class QTimerEvent;

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

    static const int DEFAULT_HEIGHT = 60; // ### remove

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
    void setMute(bool mute);
    void setPlaying(bool playing);
    void loadMotionPeriods(QnResourcePtr resource, QRegion region);

Q_SIGNALS:
    void exportRange(qint64 begin, qint64 end);
    void playbackMaskChanged(const QnTimePeriodList& playbackMask);

protected:
    void timerEvent(QTimerEvent* event);
    void updateSlider();
    bool updateRecPeriodList(bool force);

    void smartSeek(qint64 timeMSec);
    void setActualCamera(CLVideoCamera *camera);
    void updateActualCamera();
    void repaintMotionPeriods();

private Q_SLOTS:
    void setLiveMode(bool value);
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

    void setInfoText(const QString &infoText);
    void restoreInfoText();

    void onTimePeriodLoaded(const QnTimePeriodList &timePeriods, int handle);
    void onTimePeriodLoadFailed(int status, int handle);

    void onMotionPeriodLoaded(const QnTimePeriodList& timePeriods, int handle);
    void onMotionPeriodLoadFailed(int status, int handle);

    void onMrsButtonClicked();
    void updateMotionPeriods(const QnTimePeriod& period);
protected:
#ifdef EMULATE_CLUnMovedInteractiveOpacityItem
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
#endif
    void wheelEvent(QGraphicsSceneWheelEvent *) {} // ### hack to avoid scene move up and down

private:
    struct MotionPeriodLoader {
        MotionPeriodLoader(): loadingHandle(0) {}
        QnTimePeriodUpdaterPtr loader;
        int loadingHandle;
        QnTimePeriodList periods;
        QRegion region;
    };

    TimeSlider *m_timeSlider;
    ImageButton *m_backwardButton;
    ImageButton *m_stepBackwardButton;
    ImageButton *m_playButton;
    ImageButton *m_stepForwardButton;
    ImageButton *m_forwardButton;
    ImageButton *m_liveButton;
    ImageButton *m_mrsButton;
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
    int m_fullTimePeriodHandle;
    qint64 m_timePeriodUpdateTime;
    bool m_forceTimePeriodLoading;

    typedef QMap<QnNetworkResourcePtr, MotionPeriodLoader> MotionPeriods;
    MotionPeriods m_motionPeriodLoader;

    QnTimePeriod m_timePeriod;
    QnTimePeriodList m_mergedMotionPeriods;

    //QnTimePeriodUpdater *m_fullTimePeriodUpdater;

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
