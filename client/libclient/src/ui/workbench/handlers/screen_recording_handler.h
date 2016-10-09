
#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <recording/stream_recorder.h>
#include <ui/workbench/workbench_context_aware.h>

class QGLWidget;
class QnGraphicsMessageBox;
class QnDesktopDataProviderWrapper;

class QnScreenRecorder: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnWorkbenchContextAware;

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

signals:
    void recordingStarted();

    void recordingFinished(const QString &recordedFileName);

    void error(const QString &errorReason);

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
    void startRecording();

private:
    void cleanupRecorder();

    // Starts real screen recording
    void startRecordingInternal();

    void onRecordingAnimationFinished();

    void onRecordingFinished(const QString& fileName);

    void onStreamRecordingFinished(
        const QnStreamRecorder::ErrorStruct& status,
        const QString& filename);

private:
    bool m_recording;
    QnDesktopDataProviderWrapper* m_dataProvider;
    QnStreamRecorder* m_recorder;
    QnGraphicsMessageBox *m_recordingCountdownLabel;
};
