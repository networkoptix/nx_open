#ifndef QN_SCREEN_RECORDER_H
#define QN_SCREEN_RECORDER_H

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include "recording/stream_recorder.h"

class QGLWidget;
class QnDesktopDataProviderWrapper;
class QnStreamRecorder;

class QnScreenRecorder: public QObject {
    Q_OBJECT;
public:
    /**
     * Constructor.
     * 
     * \param parent                    Parent object.
     */
    QnScreenRecorder(QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnScreenRecorder();

    /**
     * \returns                         Whether screen recording is supported. 
     */
    static bool isSupported();

    /**
     * \returns                         Whether recording is in progress.
     */
    bool isRecording() const;

    /**
     * Stops screen recording.
     */
    void stopRecording();

public slots:
    /**
     * Starts screen recording.
     */
    void startRecording();

signals:
    void recordingStarted();
    void recordingFinished(const QString &recordedFileName);
    void error(const QString &errorMessage);
private slots:
    void at_recorder_recordingFinished(
        const QnStreamRecorder::ErrorStruct &status, 
        const QString                       &filename
    );
private:
    void cleanupRecorder();
private:
    bool m_recording;
    QnDesktopDataProviderWrapper* m_dataProvider;
    QnStreamRecorder* m_recorder;
};

#endif // QN_SCREEN_RECORDER_H
