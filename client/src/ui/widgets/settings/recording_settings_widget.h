#ifndef VIDEORECORDINGDIALOG_H
#define VIDEORECORDINGDIALOG_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <ui/screen_recording/video_recorder_settings.h>
#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>


namespace Ui {
    class RecordingSettings;
}

class QnRecordingSettingsWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;

public:
    explicit QnRecordingSettingsWidget(QWidget *parent = NULL);
    virtual ~QnRecordingSettingsWidget();

    Qn::DecoderQuality decoderQuality() const;
    void setDecoderQuality(Qn::DecoderQuality q);

    Qn::Resolution resolution() const;
    void setResolution(Qn::Resolution r);

    QString primaryAudioDeviceName() const;
    void setPrimaryAudioDeviceName(const QString &name);

    QString secondaryAudioDeviceName() const;
    void setSecondaryAudioDeviceName(const QString &name);

    virtual void submitToSettings() override;
    virtual void updateFromSettings() override;
signals:
    void recordingSettingsChanged();
private:
    void additionalAdjustSize();

private slots:
    void onComboboxChanged(int index);
    
    void updateRecordingWarning();

    void at_browseRecordingFolderButton_clicked();

private:
    QScopedPointer<Ui::RecordingSettings> ui;
    QnVideoRecorderSettings *m_settings;
};

#endif // VIDEORECORDINGDIALOG_H
