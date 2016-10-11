#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QElapsedTimer>

#include <recording/stream_recorder.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

class QGLWidget;
class QnCountdownTimer;
class QnGraphicsMessageBox;
class QnDesktopDataProviderWrapper;

class QnScreenRecorder: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    /*
     * Constructor.
     * \param parent Parent object.
     */
    QnScreenRecorder(QObject *parent = nullptr);

    /*
     * Virtual destructor.
     */
    virtual ~QnScreenRecorder();

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

    void onRecordingFinished(const QString& fileName);

    void onStreamRecordingFinished(
        const QnStreamRecorder::ErrorStruct& status,
        const QString& filename);

    void onError(const QString& reason);

private:
    bool m_recording;
    int m_timerId;

    QScopedPointer<QnDesktopDataProviderWrapper> m_dataProvider;
    QScopedPointer<QnStreamRecorder> m_recorder;
    QElapsedTimer m_countdown;
    QPointer<QnGraphicsMessageBox> m_messageBox;
};
