#pragma once

#include <QtCore/QObject>

class QnMjpegSessionPrivate;

class QnMjpegSession : public QObject {
    Q_OBJECT

public:
    enum State {
        Stopped,
        Connecting,
        Playing
    };

    QnMjpegSession(QObject *parent = nullptr);
    ~QnMjpegSession();

    QUrl url() const;
    void setUrl(const QUrl &url);

    State state() const;

    bool dequeueFrame(QImage *image, int *presentationTime);

signals:
    void frameEnqueued();
    void stateChanged();
    void urlChanged();

public slots:
    void start();
    void stop();

private:
    QScopedPointer<QnMjpegSessionPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnMjpegSession)
};
