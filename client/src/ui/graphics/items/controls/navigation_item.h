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

private slots:
    void updateButtonsSyncState();
    void updateButtonsSyncEffectiveState();
    void updateButtonsLiveState();
    void updateButtonsLiveSupportedState();
    void updateButtonsPlayingState();
    void updateButtonsPlayingSupportedState();
    void updateButtonsMuteState();
    void updateNavigatorSpeedFromSpeedSlider();
    void updateSpeedSliderSpeedFromNavigator();
    void updateSpeedSliderParametersFromNavigator();
    void updateButtonsSpeedState();

    void at_navigator_currentWidgetAboutToBeChanged();
    void at_navigator_currentWidgetChanged();
    void at_liveButton_clicked();
    void at_syncButton_clicked();
    void at_stepBackwardButton_clicked();
    void at_stepForwardButton_clicked();

protected:
    virtual void wheelEvent(QGraphicsSceneWheelEvent *) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *) override;

private:
    QnImageButtonWidget *m_jumpBackwardButton;
    QnImageButtonWidget *m_stepBackwardButton;
    QnImageButtonWidget *m_playButton;
    QnImageButtonWidget *m_stepForwardButton;
    QnImageButtonWidget *m_jumpForwardButton;
    QnImageButtonWidget *m_liveButton;
    QnImageButtonWidget *m_mrsButton;
    QnImageButtonWidget *m_syncButton;
    QnImageButtonWidget *m_muteButton;

    GraphicsLabel *m_timeLabel;

    bool m_updatingNavigatorFromSpeedSlider;
    bool m_updatingSpeedSliderFromNavigator;

    QnTimeSlider *m_timeSlider;
    QnTimeScrollBar *m_timeScrollBar;
    QnSpeedSlider *m_speedSlider;
    QnVolumeSlider *m_volumeSlider;

    QnWorkbenchNavigator *m_navigator;
};

#endif // NAVIGATIONITEM_H
