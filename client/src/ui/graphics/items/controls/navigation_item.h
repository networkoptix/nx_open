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

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void updateSyncButtonChecked();
    void updateSyncButtonEnabled();
    void updateLiveButtonChecked();
    void updateLiveButtonEnabled();
    void updatePlaybackButtonsIcons();
    void updatePlaybackButtonsEnabled();
    void updateMuteButtonChecked();
    void updateNavigatorSpeedFromSpeedSlider();
    void updateSpeedSliderSpeedFromNavigator();
    void updateSpeedSliderParametersFromNavigator();
    void updatePlaybackButtonsPressed();
    void updatePlayButtonChecked();

    bool at_speedSlider_wheelEvent(QGraphicsSceneWheelEvent *event);
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
    QnImageButtonWidget *m_thumbnailsButton;

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
