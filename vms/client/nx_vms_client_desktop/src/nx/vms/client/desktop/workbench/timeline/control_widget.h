// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QPushButton>

#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop::workbench::timeline {

class VolumeSlider;

class ControlWidget: public QWidget, public WindowContextAware
{
    Q_OBJECT

    using base_type = QWidget;

    using CustomPaintedButton = CustomPainted<QPushButton>;

public:
    explicit ControlWidget(WindowContext* context, QWidget* parent = nullptr);

    void setTooltipsVisible(bool enabled);

signals:
    void geometryChanged();

private:
    void initButton(
        CustomPaintedButton* button,
        menu::IDType actionType,
        const QString& iconPath,
        const QString& checkedIconPath = "",
        bool connectToAction = true);

    void updateSyncButtonState();
    void updateLiveButtonState();
    void updateVolumeButtonsEnabled();
    void updateMuteButtonChecked();
    void updateBookButtonEnabled();

    void at_jumpToliveAction_triggered();
    void at_toggleSyncAction_triggered();

private:
    VolumeSlider* m_volumeSlider;
    CustomPaintedButton* m_muteButton;

    CustomPaintedButton* m_liveButton;
    CustomPaintedButton* m_syncButton;
    CustomPaintedButton* m_thumbnailsButton;
    CustomPaintedButton* m_calendarButton;
};

} // nx::vms::client::desktop::workbench::timeline
