// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/vms/client/desktop/resource/screen_recording/video_recorder_settings.h> //< TODO: #sivanov Move out enums.
#include <ui/widgets/common/abstract_preferences_widget.h>

namespace Ui {
class RecordingSettings;
}

class QnDwm;
namespace nx::vms::client::desktop { class VideoRecorderSettings; }

class QnRecordingSettingsWidget : public QnAbstractPreferencesWidget
{
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;

public:
    explicit QnRecordingSettingsWidget(
        nx::vms::client::desktop::VideoRecorderSettings* recorderSettings,
        QWidget* parent = nullptr);
    virtual ~QnRecordingSettingsWidget();

    virtual void applyChanges() override;
    virtual void loadDataToUi() override;
    virtual bool hasChanges() const override;

signals:
    void recordingSettingsChanged();

private:
    void initScreenComboBox();
    void initQualityCombobox();
    void initResolutionCombobox();

    Qn::CaptureMode captureMode() const;
    void setCaptureMode(Qn::CaptureMode c);

    Qn::DecoderQuality decoderQuality() const;
    void setDecoderQuality(Qn::DecoderQuality q);

    Qn::Resolution resolution() const;
    void setResolution(Qn::Resolution r);

    int screen() const;
    void setScreen(int screen);

    bool isPrimaryScreenSelected() const;

private:
    void updateRecordingWarning();
    void updateDisableAeroCheckbox();

    void at_browseRecordingFolderButton_clicked();
    void at_dwm_compositionChanged();

private:
    QScopedPointer<Ui::RecordingSettings> ui;
    nx::vms::client::desktop::VideoRecorderSettings* m_settings;
    QnDwm* m_dwm;
};
