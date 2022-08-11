// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QElapsedTimer>

#include <recording/stream_recorder_data.h>
#include <ui/workbench/workbench_context_aware.h>

class QnCountdownTimer;

namespace nx::vms::client::desktop {

class DesktopDataProviderWrapper;
class ExportStorageStreamRecorder;
class SceneBanner;

} // namespace nx::vms::client::desktop

class QnWorkbenchScreenRecordingHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    /*
     * Constructor.
     * \param parent Parent object.
     */
    QnWorkbenchScreenRecordingHandler(QObject *parent = nullptr);

    /*
     * Virtual destructor.
     */
    virtual ~QnWorkbenchScreenRecordingHandler();

protected:
    virtual void timerEvent(QTimerEvent* event) override;

private:
    /*
    * \returns                         Whether recording is in progress.
    */
    bool isRecording() const;

    /*
    * Stops screen recording.
    */
    void stopRecording();
    /*
    * Initiates screen recording.
    */
    void startRecordingCountdown();
    void stopRecordingCountdown();

    bool isRecordingCountdown() const;

    QString recordingCountdownText(int seconds) const;

    void stopRecordingInternal();

    // Starts real screen recording
    void startRecordingInternal();

    void onRecordingFinished();

    void onStreamRecordingStarted();
    void onStreamRecordingFinished(
        const std::optional<nx::recording::Error>& status,
        const QString& filename);

    void onProviderFinished();

    void onError(const QString& reason);

private:
    bool m_recording;
    QString m_recordedFileName;
    int m_timerId;

    QScopedPointer<nx::vms::client::desktop::DesktopDataProviderWrapper> m_dataProvider;
    QScopedPointer<nx::vms::client::desktop::ExportStorageStreamRecorder> m_recorder;
    QElapsedTimer m_countdown;
    QPointer<nx::vms::client::desktop::SceneBanner> m_messageBox;
};
