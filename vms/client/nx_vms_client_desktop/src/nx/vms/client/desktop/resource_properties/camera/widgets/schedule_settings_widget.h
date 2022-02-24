// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/utils/impl_ptr.h>

namespace Ui { class ScheduleSettingsWidget; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;
class ScheduleCellPainter;

class ScheduleSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit ScheduleSettingsWidget(QWidget* parent = nullptr);
    virtual ~ScheduleSettingsWidget() override;

    void setStore(CameraSettingsDialogStore* store);
    void setScheduleCellPainter(ScheduleCellPainter* cellPainter);

    // Utility functions can be used by parent widget.
    static QString motionOptionHint(const CameraSettingsDialogState& state);
    static QString disabledMotionDetectionTooltip(const CameraSettingsDialogState& state);
    static QString disabledMotionPlusLqTooltip(const CameraSettingsDialogState& state);
    static QString disabledObjectDetectionTooltip(const CameraSettingsDialogState& state);
    static QString disabledMotionAndObjectDetectionTooltip(const CameraSettingsDialogState& state);

private:
    void setupUi();
    void loadState(const CameraSettingsDialogState& state);

    void setupRecordingAndMetadataTypeControls(const CameraSettingsDialogState& state);

private:
    nx::utils::ImplPtr<Ui::ScheduleSettingsWidget> ui;
};

} // namespace nx::vms::client::desktop
