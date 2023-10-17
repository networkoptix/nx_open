// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QPushButton>

#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop::workbench::timeline {

class SpeedSlider;

class NavigationWidget: public QWidget, public WindowContextAware
{
    Q_OBJECT

    using base_type = QWidget;

    using CustomPaintedButton = CustomPainted<QPushButton>;

public:
    explicit NavigationWidget(WindowContext* context, QWidget* parent = nullptr);
    virtual ~NavigationWidget() override = default;

    void setTooltipsVisible(bool enabled);

signals:
    void geometryChanged();

private:
    void initButton(
        CustomPaintedButton* button,
        menu::IDType actionType,
        const QString& iconPath,
        const QString& checkedIconPath = "");
    void updatePlaybackButtonsIcons();
    void updatePlaybackButtonsEnabled();
    void updateJumpButtonsTooltips();
    void updatePlayButtonChecked();
    void updateNavigatorSpeedFromSpeedSlider();
    void updateSpeedSliderSpeedFromNavigator();
    void updateSpeedSliderParametersFromNavigator();
    void updateBackwardButtonEnabled();
    void updatePlaybackButtonsTooltips();

    void showMessage(const QString& text);

    void at_stepBackwardButton_clicked();
    void at_stepForwardButton_clicked();

private:
    SpeedSlider* m_speedSlider;

    CustomPaintedButton* m_jumpBackwardButton;
    CustomPaintedButton* m_stepBackwardButton;
    CustomPaintedButton* m_playButton;
    CustomPaintedButton* m_stepForwardButton;
    CustomPaintedButton* m_jumpForwardButton;

    QString m_rewindDisabledText;

    bool m_updatingSpeedSliderFromNavigator = false;
    bool m_updatingNavigatorFromSpeedSlider = false;
    QSet<QnUuid> m_messages;
};

} // nx::vms::client::desktop::workbench::timeline
