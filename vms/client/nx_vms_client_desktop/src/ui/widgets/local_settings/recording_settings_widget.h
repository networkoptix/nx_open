// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/vms/client/desktop/resource/screen_recording/types.h>
#include <ui/widgets/common/abstract_preferences_widget.h>

namespace Ui { class RecordingSettingsWidget; }

class QnDwm;

namespace nx::vms::client::desktop {

class RecordingSettingsWidget: public QnAbstractPreferencesWidget
{
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;

public:
    explicit RecordingSettingsWidget(QWidget* parent = nullptr);
    virtual ~RecordingSettingsWidget();

    virtual void applyChanges() override;
    virtual void loadDataToUi() override;
    virtual bool hasChanges() const override;

signals:
    void recordingSettingsChanged();

private:
    void initScreenComboBox();
    void initQualityCombobox();
    void initResolutionCombobox();

    screen_recording::CaptureMode captureMode() const;
    void setCaptureMode(screen_recording::CaptureMode c);

    screen_recording::Quality quality() const;
    void setQuality(screen_recording::Quality q);

    screen_recording::Resolution resolution() const;
    void setResolution(screen_recording::Resolution resolution);

    int screen() const;
    void setScreen(int screen);

    bool isPrimaryScreenSelected() const;

private:
    void updateRecordingWarning();
    void updateDisableAeroCheckbox();

    void at_browseRecordingFolderButton_clicked();
    void at_dwm_compositionChanged();

private:
    QScopedPointer<Ui::RecordingSettingsWidget> ui;
    QnDwm* m_dwm;
};

} // namespace nx::vms::client::desktop
