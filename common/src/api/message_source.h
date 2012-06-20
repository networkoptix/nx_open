#ifndef QN_MESSAGE_SOURCE
#define QN_MESSAGE_SOURCE

#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QTextStream>
#include <QtCore/QVariant>
#include <QtCore/QQueue>
#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtCore/QMetaType>
#include <QtCore/QScopedPointer>
#include <QtNetwork/QNetworkAccessManager>

#include <qjson/parser.h>

#include "message.h"

class QnJsonStreamParser;
class QnPbStreamParser;

class QnMessageSource : public QObject
{
    Q_OBJECT

public:
    QnMessageSource(QUrl url, int retryTimeout = 3000);
    virtual ~QnMessageSource();

    void stop();

signals:
    void stopped();
    void messageReceived(QnMessage message);
    void connectionOpened();
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
    QUrl m_url;
    int m_retryTimeout;
    QNetworkAccessManager m_manager;
    QNetworkReply *m_reply;

    quint32 m_seqNumber;
    QTime m_eventWaitTimer;
    QTimer m_pingTimer;

    QScopedPointer<QnPbStreamParser> m_streamParser;
};

#endif // _QN_EVENT_SOURCE_
