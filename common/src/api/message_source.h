#ifndef QN_MESSAGE_SOURCE
#define QN_MESSAGE_SOURCE

#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QTextStream>
#include <QtCore/QVariant>
#include <QtCore/QQueue>
#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <QtCore/QMetaType>
#include <QtCore/QScopedPointer>
#include <QtNetwork/QNetworkAccessManager>

#include "message.h"

class QnPbStreamParser;

class QnMessageSource : public QObject
{
    Q_OBJECT

public:
    QnMessageSource(QUrl url, int retryTimeout = 3000);
    virtual ~QnMessageSource();

    void setAuthKey(const QString& authKey);
    void setVideoWallKey(const QString& videoWallKey);
    void stop();

signals:
    void stopped();
    void messageReceived(QnMessage message);
    void connectionOpened(QnMessage message);
    void connectionClosed(QString errorString);
    void connectionReset();

public slots:
    void doStop();
    void startRequest();

private slots:
    void onPingTimer();

    void httpFinished();
    void httpReadyRead();
    void slotAuthenticationRequired(QNetworkReply *, QAuthenticator *);
#ifndef QT_NO_OPENSSL
    void sslErrors(QNetworkReply *, const QList<QSslError> &errors);
#endif

private:
    bool m_timeoutFlag;
    QUrl m_url;
    int m_retryTimeout;
    QString m_authKey;
    QString m_videoWallKey;
    QNetworkAccessManager m_manager;
    QNetworkReply *m_reply;

    quint32 m_seqNumber;
    QElapsedTimer m_eventWaitTimer;
    QTimer m_pingTimer;

    QScopedPointer<QnPbStreamParser> m_streamParser;
};

#endif // _QN_EVENT_SOURCE_
