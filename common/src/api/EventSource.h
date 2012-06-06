#ifndef _QN_EVENT_SOURCE_
#define _QN_EVENT_SOURCE_

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
#include <QtNetwork/QNetworkAccessManager>

#include <qjson/parser.h>

#include "utils/common/qnid.h"

static const char* QN_EVENT_EMPTY               = "EE";
static const char* QN_EVENT_PING                = "PE";
static const char* QN_EVENT_RES_CHANGE          = "RC";
static const char* QN_EVENT_RES_STATUS_CHANGE   = "RSC";
static const char* QN_EVENT_RES_DISABLED_CHANGE = "RDC";
static const char* QN_EVENT_RES_DELETE          = "RD";
static const char* QN_EVENT_RES_SETPARAM        = "RSP";
static const char* QN_EVENT_LICENSE_CHANGE      = "LC";
static const char* QN_CAMERA_SERVER_ITEM        = "CSI";

struct QnEvent
{
    QString eventType;
    quint32 seqNumber;

    QString objectName;
    QnId objectId;
    QnId parentId;
    QString resourceGuid;
    QString data;

    // for RSP event
    QString paramName;
    QString paramValue;

    QMap<QString, QVariant> dict;

    bool load(const QVariant& parsed);
	QString objectNameLower() const;

    static quint32 nextSeqNumber(quint32 seqNumber);
};
Q_DECLARE_METATYPE(QnEvent);

class QnJsonStreamParser
{
public:
    void addData(const QByteArray& data);
    bool nextMessage(QVariant& parsed);

private:
    QQueue<QByteArray> blocks;
    QByteArray incomplete;

    QJson::Parser parser;
};

class QnEventSource : public QObject
{
    Q_OBJECT

public:
    QnEventSource(QUrl url, int retryTimeout = 3000);

    void stop();

signals:
    void stopped();
    void eventReceived(QnEvent event);
    void connectionOpened();
    void connectionClosed(QString errorString);
    void connectionReset();

public slots:
    void doStop();
    void startRequest();

private slots:
    void onPingTimer();

    // void slotError(QNetworkReply::NetworkError code);
    void httpFinished();
    void httpReadyRead();
    void slotAuthenticationRequired(QNetworkReply*,QAuthenticator *);
#ifndef QT_NO_OPENSSL
    void sslErrors(QNetworkReply*,const QList<QSslError> &errors);
#endif

private:
    QUrl m_url;
    int m_retryTimeout;
    QNetworkAccessManager m_manager;
    QNetworkReply *m_reply;

    quint32 m_seqNumber;
    QTime m_eventWaitTimer;
    QTimer m_pingTimer;

    QnJsonStreamParser m_streamParser;
};

#endif // _QN_EVENT_SOURCE_
