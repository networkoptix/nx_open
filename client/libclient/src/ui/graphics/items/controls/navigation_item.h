#ifndef QN_NAVIGATION_ITEM_H
#define QN_NAVIGATION_ITEM_H

#include <ui/graphics/items/generic/framed_widget.h>
#include <ui/workbench/workbench_context_aware.h>

class QnSpeedSlider;
class QnVolumeSlider;
class QnImageButtonWidget;
class QnTimeSlider;
class QnTimeScrollBar;
class QnWorkbenchNavigator;
class QGraphicsProxyWidget;

class QnNavigationItem : public QnFramedWidget, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QnFramedWidget base_type;

public:
    explicit QnNavigationItem(QGraphicsItem* parent = NULL);
    ~QnNavigationItem();

    QnTimeSlider* timeSlider() const            { return m_timeSlider;     }
    QnSpeedSlider* speedSlider() const          { return m_speedSlider;    }
    QnVolumeSlider* volumeSlider() const        { return m_volumeSlider;   }
    QnTimeScrollBar* timeScrollBar() const      { return m_timeScrollBar;  }

    QnImageButtonWidget* calendarButton() const { return m_calendarButton; }

    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void updateSyncButtonChecked();
    void updateSyncButtonEnabled();
    void updateLiveButtonChecked();
    void updateLiveButtonEnabled();
    void updatePlaybackButtonsIcons();
    void updatePlaybackButtonsEnabled();
    void updateVolumeButtonsEnabled();
    void updateMuteButtonChecked();
    void updateNavigatorSpeedFromSpeedSlider();
    void updateSpeedSliderSpeedFromNavigator();
    void updateSpeedSliderParametersFromNavigator();
    void updatePlaybackButtonsPressed();
    void updatePlayButtonChecked();
    void updateJumpButtonsTooltips();
    void updateBookButtonEnabled();

    bool at_speedSlider_wheelEvent(QGraphicsSceneWheelEvent* event);
    void at_liveButton_clicked();
    void at_syncButton_clicked();
    void at_stepBackwardButton_clicked();
    void at_stepForwardButton_clicked();

protected:
    virtual void wheelEvent(QGraphicsSceneWheelEvent* event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

private:
    QnImageButtonWidget* newActionButton(QnActions::IDType id);

    bool isTimelineRelevant() const;

private:
    QnImageButtonWidget* m_jumpBackwardButton;
    QnImageButtonWidget* m_stepBackwardButton;
    QnImageButtonWidget* m_playButton;
    QnImageButtonWidget* m_stepForwardButton;
    QnImageButtonWidget* m_jumpForwardButton;
    QnImageButtonWidget* m_muteButton;

    QnImageButtonWidget* m_liveButton;
    QnImageButtonWidget* m_syncButton;
    QnImageButtonWidget* m_bookmarksModeButton;
    QnImageButtonWidget* m_calendarButton;
    QnImageButtonWidget* m_thumbnailsButton;

    bool m_updatingSpeedSliderFromNavigator;
    bool m_updatingNavigatorFromSpeedSlider;

    QnTimeSlider* m_timeSlider;
    QnTimeScrollBar* m_timeScrollBar;
    QnSpeedSlider* m_speedSlider;
    QnVolumeSlider* m_volumeSlider;
    QnFramedWidget* m_separators;
};

#endif // QN_NAVIGATION_ITEM_H
