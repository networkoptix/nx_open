
#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

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
    void startRecodingCountdown();

private:
    void cleanupRecordingStuff();

    // Starts real screen recording
    void startRecordingInternal();

    void onRecordingFinished(const QString& fileName);

    void onStreamRecordingFinished(
        const QnStreamRecorder::ErrorStruct& status,
        const QString& filename);

    void onError(const QString& reason);

private:
    bool m_recording;
    int m_countdownCounters;

    QScopedPointer<QnDesktopDataProviderWrapper> m_dataProvider;
    QScopedPointer<QnStreamRecorder> m_recorder;
    QScopedPointer<QnCountdownTimer> m_countdown;
};
