#ifndef NAVIGATIONITEM_H
#define NAVIGATIONITEM_H

#include <ui/graphics/items/simple_frame_widget.h>
#include <ui/workbench/workbench_context_aware.h>

class QnSpeedSlider;
class QnVolumeSlider;
class GraphicsLabel;
class QnImageButtonWidget;
class QnTimeSlider;
class QnTimeScrollBar;
class QnWorkbenchNavigator;

class QnNavigationItem : public QnSimpleFrameWidget, public QnWorkbenchContextAware {
    Q_OBJECT;

    typedef QnSimpleFrameWidget base_type;

public:
    explicit QnNavigationItem(QGraphicsItem *parent = NULL, QnWorkbenchContext *context = NULL);
    ~QnNavigationItem();

    QnTimeSlider *timeSlider() const {
        return m_timeSlider;
    }

    QnTimeScrollBar *timeScrollBar() const {
        return m_timeScrollBar;
    }

    QnWorkbenchNavigator *navigator() const {
        return m_navigator;
    }

public slots:
    void setMute(bool mute);
    void setPlaying(bool playing);
    void onSyncButtonToggled(bool value);

signals:
    void enableItemSync(bool value);
    void clearMotionSelection();

private slots:
    void updateLiveState();

    void onLiveModeChanged(bool value);
    void pause();
    void play();
    void togglePlayPause();

    void rewindBackward();
    void rewindForward();

    void stepBackward();
    void stepForward();

    void onSpeedChanged(float);

    void onVolumeLevelChanged(int);

protected:
    virtual void wheelEvent(QGraphicsSceneWheelEvent *) override {
        /* Don't let wheel events escape into the scene. */
    }

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *) override {
        /* Prevent surprising click-through scenarios. */
    }

private:
    QnImageButtonWidget *m_backwardButton;
    QnImageButtonWidget *m_stepBackwardButton;
    QnImageButtonWidget *m_playButton;
    QnImageButtonWidget *m_stepForwardButton;
    QnImageButtonWidget *m_forwardButton;
    QnImageButtonWidget *m_liveButton;
    QnImageButtonWidget *m_mrsButton;
    QnImageButtonWidget *m_syncButton;
    QnImageButtonWidget *m_muteButton;

    GraphicsLabel *m_timeLabel;

    QnTimeSlider *m_timeSlider;
    QnTimeScrollBar *m_timeScrollBar;
    QnSpeedSlider *m_speedSlider;
    QnVolumeSlider *m_volumeSlider;

    QnWorkbenchNavigator *m_navigator;

    bool m_playing;
};

#endif // NAVIGATIONITEM_H
