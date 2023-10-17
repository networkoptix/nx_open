// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <nx/vms/client/desktop/window_context_aware.h>
#include <recording/stream_recorder_data.h>

class QnCountdownTimer;

namespace nx::vms::client::desktop {

class DesktopDataProviderWrapper;
class ExportStorageStreamRecorder;
class SceneBanner;

class ScreenRecordingActionHandler: public QObject, public WindowContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    /*
     * Constructor.
     * \param parent Parent object.
     */
    ScreenRecordingActionHandler(WindowContext* windowContext, QObject* parent = nullptr);

    /*
     * Virtual destructor.
     */
    virtual ~ScreenRecordingActionHandler();

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
    bool m_recording = false;
    QString m_recordedFileName;
    int m_timerId = 0;

    QScopedPointer<DesktopDataProviderWrapper> m_dataProvider;
    QScopedPointer<ExportStorageStreamRecorder> m_recorder;
    QElapsedTimer m_countdown;
    QPointer<SceneBanner> m_messageBox;
};

} // namespace nx::vms::client::desktop
