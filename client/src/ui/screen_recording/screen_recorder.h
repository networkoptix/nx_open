#ifndef QN_SCREEN_RECORDER_H
#define QN_SCREEN_RECORDER_H

#include <QObject>
#include <QScopedPointer>

class QGLWidget;
class QnDesktopFileEncoder;

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
    bool isSupported() const;

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

private:
    bool m_recording;
    QnDesktopFileEncoder* m_encoder;
};

#endif // QN_SCREEN_RECORDER_H
