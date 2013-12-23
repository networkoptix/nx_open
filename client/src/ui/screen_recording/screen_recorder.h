#ifndef QN_SCREEN_RECORDER_H
#define QN_SCREEN_RECORDER_H

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

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
     *
     * \param appWidget                 Widget to use for recording in windowed mode.
     */
    void startRecording(QGLWidget *appWidget);

signals:
    void recordingStarted();
    void recordingFinished(const QString &recordedFileName);
    void error(const QString &errorMessage);
private slots:
    void onRecordingFailed(QString msg);
    void onRecordingFinished(QString);
private:
    void cleanupRecorder();
private:
    bool m_recording;
    QnDesktopDataProviderWrapper* m_dataProvider;
    QnStreamRecorder* m_recorder;
    //QnDesktopFileEncoder* m_encoder;
};

#endif // QN_SCREEN_RECORDER_H
