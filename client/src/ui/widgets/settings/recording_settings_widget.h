#ifndef VIDEORECORDINGDIALOG_H
#define VIDEORECORDINGDIALOG_H

#include <QtCore/QScopedPointer>
#include <QtGui/QWidget>

#include "ui/screen_recording/video_recorder_settings.h"

namespace Ui {
    class RecordingSettings;
}

class QnRecordingSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QnRecordingSettingsWidget(QWidget *parent = NULL);
    virtual ~QnRecordingSettingsWidget();

    Qn::CaptureMode captureMode() const;
    void setCaptureMode(Qn::CaptureMode c);

    Qn::DecoderQuality decoderQuality() const;
    void setDecoderQuality(Qn::DecoderQuality q);

    Qn::Resolution resolution() const;
    void setResolution(Qn::Resolution r);

    int screen() const;
    void setScreen(int screen);

    QString primaryAudioDeviceName() const;
    void setPrimaryAudioDeviceName(const QString &name);

    QString secondaryAudioDeviceName() const;
    void setSecondaryAudioDeviceName(const QString &name);

    void updateFromSettings();
    void submitToSettings();

private:
    void additionalAdjustSize();

private slots:
    void onComboboxChanged(int index);
    void onMonitorChanged(int);
    void onDisableAeroChecked(bool);

private:
    QScopedPointer<Ui::RecordingSettings> ui;
    QnVideoRecorderSettings *m_settings;
};

#endif // VIDEORECORDINGDIALOG_H
