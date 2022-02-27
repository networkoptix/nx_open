// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

namespace Ui { class RecordingThresholdWidget; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class RecordingThresholdWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit RecordingThresholdWidget(QWidget* parent = nullptr);
    virtual ~RecordingThresholdWidget();

    void setStore(CameraSettingsDialogStore* store);

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    QScopedPointer<Ui::RecordingThresholdWidget> ui;
};

} // namespace nx::vms::client::desktop
