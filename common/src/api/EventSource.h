#ifndef _QN_EVENT_SOURCE_
#define _QN_EVENT_SOURCE_

#include <QString>
#include <QMap>
#include <QObject>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QTextStream>
#include <QVariant>
#include <QQueue>
#include <qjson/parser.h>

#include "utils/common/qnid.h"

static const char* QN_EVENT_RES_CHANGE = "RC";
static const char* QN_EVENT_RES_DELETE = "RD";
static const char* QN_EVENT_RES_SETPARAM = "RSP";

struct QnEvent
{
    QString eventType;
    QString objectName;
    QnId resourceId;

    // for RSP event
    QString paramName;
    QString paramValue;

    bool load(const QVariant& parsed);
};

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

signals:
    void eventReceived(QnEvent event);
    void connectionClosed(QString errorString);

public slots:
    void startRequest();

private slots:
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
    QNetworkAccessManager qnam;
    QNetworkReply *reply;
    int httpGetId;

    QnJsonStreamParser streamParser;
};

#endif // _QN_EVENT_SOURCE_
