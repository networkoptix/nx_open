#ifndef NAVIGATIONITEM_H
#define NAVIGATIONITEM_H

#include <QtCore/QTimer>

#include <QtGui/QGraphicsWidget>

#include <ui/graphics/items/simple_frame_widget.h>

#include "recording/time_period.h"
#include "camera/multi_camera_time_period_loader.h"


class QTimerEvent;

class CLVideoCamera;
class QnSpeedSlider;
class TimeSlider;
class QnVolumeSlider;
class GraphicsLabel;
class QnAbstractRenderer;
class QnAbstractArchiveReader;
class QnImageButtonWidget;
class QnWorkbenchDisplay;

class NavigationItem : public QnSimpleFrameWidget
{
    Q_OBJECT;

    typedef QnSimpleFrameWidget base_type;

public:
    explicit NavigationItem(QnWorkbenchDisplay *display, QGraphicsItem *parent = 0);
    ~NavigationItem();

    void setVideoCamera(CLVideoCamera* camera);
    void addReserveCamera(CLVideoCamera* camera);
    void removeReserveCamera(CLVideoCamera* camera);

    CLVideoCamera *videoCamera() const { return m_camera; }
    CLVideoCamera *forcedVideoCamera() const { return m_forcedCamera; }

    inline bool isPlaying() const { return m_playing; }

public Q_SLOTS:
    void setMute(bool mute);
    void setPlaying(bool playing);
    void loadMotionPeriods(QnResourcePtr resource, QnAbstractArchiveReader* reader, QList<QRegion> regions);
    void onSyncButtonToggled(bool value);
    void onDisplayingStateChanged(QnResourcePtr, bool);


Q_SIGNALS:
    void exportRange(CLVideoCamera* camera, qint64 begin, qint64 end);
    //void playbackMaskChanged(const QnTimePeriodList& playbackMask);
    void enableItemSync(bool value);
    void clearMotionSelection();
    void actualCameraChanged(CLVideoCamera *camera);

protected:
    virtual void timerEvent(QTimerEvent* event) override;
    void updateSlider();
    bool updateRecPeriodList(bool force);

    void smartSeek(qint64 timeMSec);
    void setActualCamera(CLVideoCamera *camera);
    void updateActualCamera();
    void repaintMotionPeriods();

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

    void onTimePeriodLoaded(const QnTimePeriodList &timePeriods, int handle);
    void onTimePeriodLoadFailed(int status, int handle);

    void onMotionPeriodLoaded(const QnTimePeriodList& timePeriods, int handle);
    void onMotionPeriodLoadFailed(int status, int handle);

    void updateMotionPeriods(const QnTimePeriod& period);

    void at_liveButton_clicked(bool checked);
    
    void onExportRange(qint64 startTimeMs,qint64 endTimeMs);

protected:
    virtual void wheelEvent(QGraphicsSceneWheelEvent *) override {
        /* Don't let wheel events escape into the scene. */
    }

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *) override {
        /* Prevent surprising click-through scenarios. */
    }

    CLVideoCamera* findCameraByResource(QnResourcePtr resource);

private:
    struct MotionPeriodLoader {
        MotionPeriodLoader(): loadingHandle(0), reader(0) {}
        QnTimePeriodLoader *loader;
        int loadingHandle;
        QnTimePeriodList periods;
        QList<QRegion> regions;
        QnAbstractArchiveReader* reader;

        bool isMotionEmpty() const;
    };
    MotionPeriodLoader* getMotionLoader(QnAbstractArchiveReader* reader);

    TimeSlider *m_timeSlider;
    QnImageButtonWidget *m_backwardButton;
    QnImageButtonWidget *m_stepBackwardButton;
    QnImageButtonWidget *m_playButton;
    QnImageButtonWidget *m_stepForwardButton;
    QnImageButtonWidget *m_forwardButton;
    QnImageButtonWidget *m_liveButton;
    QnImageButtonWidget *m_mrsButton;
    QnImageButtonWidget *m_syncButton;
    QnSpeedSlider *m_speedSlider;
    QnImageButtonWidget *m_muteButton;
    QnVolumeSlider *m_volumeSlider;
    GraphicsLabel *m_timeLabel;

    QHash<CLVideoCamera *, qreal> m_zoomByCamera;
    CLVideoCamera *m_camera;
    CLVideoCamera *m_forcedCamera;
    QSet<CLVideoCamera *> m_reserveCameras;
    int m_timerId;
    qint64 m_currentTime;
    bool m_playing;
    int m_fullTimePeriodHandle;
    qint64 m_timePeriodUpdateTime;
    bool m_forceTimePeriodLoading;

    typedef QMap<QnResourcePtr, MotionPeriodLoader> MotionPeriods;
    MotionPeriods m_motionPeriodLoader;

    QnTimePeriod m_timePeriod;
    QnTimePeriod m_motionPeriod;
    QnTimePeriodList m_mergedMotionPeriods;
    int m_timePeriodLoadErrors;

    //QnTimePeriodUpdater *m_fullTimePeriodUpdater;
};

#endif // NAVIGATIONITEM_H
