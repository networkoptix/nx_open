#pragma once

#include <QtCore/QObject>

class QnMjpegSessionPrivate;

class QnMjpegSession : public QObject {
    Q_OBJECT

public:
    enum State {
        Stopped,
        Connecting,
        Playing,
        Paused
    };

    QnMjpegSession(QObject *parent);
    ~QnMjpegSession();

    QUrl url() const;
    void setUrl(const QUrl &url);

    State state() const;

    bool dequeueFrame(QByteArray *data, int *displayTime);

signals:
    void frameEnqueued();
    void stateChanged();
    void urlChanged();

public slots:
    void play();
    void stop();
    void pause();

private:
    QScopedPointer<QnMjpegSessionPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnMjpegSession)
};
