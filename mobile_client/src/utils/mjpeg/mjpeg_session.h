#pragma once

#include <QtCore/QObject>

class QnMjpegSessionPrivate;

class QnMjpegSession : public QObject {
    Q_OBJECT

public:
    enum State {
        Stopped,
        Connecting,
        Disconnecting,
        Playing
    };

    struct FrameData
    {
        int presentationTime;
        qint64 timestamp;
        QImage image;

        FrameData();

        bool isNull() const;
        void clear();
    };

    QnMjpegSession(QObject *parent = nullptr);
    ~QnMjpegSession();

    QUrl url() const;
    void setUrl(const QUrl &url);

    State state() const;

    FrameData getFrame();

    qint64 finalTimestampMs() const;
    void setFinalTimestampMs(qint64 finalTimestampMs);

signals:
    void frameAvailable();
    void stateChanged();
    void urlChanged();
    void finished();
    void finalTimestampChanged();

public slots:
    void start();
    void stop();

private:
    QScopedPointer<QnMjpegSessionPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnMjpegSession)
};
