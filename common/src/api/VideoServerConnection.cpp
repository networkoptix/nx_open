
#include <QDebug>
#include <QNetworkReply>
#include <QDomDocument>
#include <QXmlInputSource>
#include <QSharedPointer>
#include "QStdIStream.h"
#include "utils/common/util.h"

#include "VideoServerConnection.h"
#include "VideoServerConnection_p.h"
#include "SessionManager.h"
#include "api/serializer/serializer.h"
#include "../src/network/kernel/qnetworkproxy.h"

QString QnVideoServerConnection::m_proxyAddr;
int QnVideoServerConnection::m_proxyPort = 0;

// ----------------------- QMyNetworkProxyFactory ---------------------------

class QMyNetworkProxyFactory: public QNetworkProxyFactory
{
public:
    QMyNetworkProxyFactory()
    {
        setApplicationProxyFactory(this);
    }
    ~QMyNetworkProxyFactory()
    {
        setApplicationProxyFactory(0);
    }

    void addToProxyList(const QUrl& url)
    {
        m_proxyInfo.insert(url);
    }

    void clearProxyList()
    {
        m_proxyInfo.clear();
    }

    static QMyNetworkProxyFactory* instance();
protected:
    virtual QList<QNetworkProxy> queryProxy ( const QNetworkProxyQuery & query = QNetworkProxyQuery() ) override
    {
        QList<QNetworkProxy> rez;
        if (QnVideoServerConnection::getProxyPort() == 0 || query.url().queryItems().isEmpty()) {
            rez << QNetworkProxy(QNetworkProxy::NoProxy);
            return rez;
        }
        QString host = query.peerHostName();
        int peerPort = query.peerPort();
        QUrl url = query.url();
        url.setPath("");
        url.setUserInfo("");
        url.setQueryItems(QList<QPair<QString, QString> > ());
        QSet<QUrl>::const_iterator itr = m_proxyInfo.find(url);
        if (itr == m_proxyInfo.end())
            rez << QNetworkProxy(QNetworkProxy::NoProxy);
        else 
            rez << QNetworkProxy(QNetworkProxy::HttpProxy, QnVideoServerConnection::getProxyHost(), QnVideoServerConnection::getProxyPort());
        return rez;
    }
private:
    QSet<QUrl> m_proxyInfo;
};

Q_GLOBAL_STATIC(QMyNetworkProxyFactory, inst)

QMyNetworkProxyFactory* QMyNetworkProxyFactory::instance()
{
    return inst();
}


namespace {
    // for video server methods
    QnTimePeriodList parseBinaryTimePeriods(const QByteArray &reply)
    {
        QnTimePeriodList result;

        QByteArray localReply = reply;
        QBuffer buffer(&localReply);
        buffer.open(QIODevice::ReadOnly);

        QStdIStream is(&buffer);

        char format[3];
        buffer.read(format, 3);
        if (QByteArray(format, 3) != "BIN")
            return result;
        while (buffer.size() - buffer.pos() >= 12)
        {
            qint64 startTimeMs = 0;
            qint64 durationMs = 0;
            buffer.read(((char*) &startTimeMs), 6);
            buffer.read(((char*) &durationMs), 6);
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
            startTimeMs <<= 16;
            durationMs <<= 16;
#else
            startTimeMs >>= 16;
            durationMs >>= 16;
#endif
            result << QnTimePeriod(ntohll(startTimeMs), ntohll(durationMs));
        }

        return result;
    }
} // anonymous namespace

void detail::QnVideoServerConnectionReplyProcessor::at_replyReceived(int status, const QnTimePeriodList &result, int handle)
{
    emit finished(status, result, handle);
    deleteLater();
}

// ---------------------------------- QnVideoServerConnection ---------------------

QnVideoServerConnection::QnVideoServerConnection(const QUrl &url, QObject *parent):
    QObject(parent),
    m_url(url)
{
    QMyNetworkProxyFactory::instance()->addToProxyList(m_url);
}

QnVideoServerConnection::~QnVideoServerConnection() {}

QnRequestParamList QnVideoServerConnection::createParamList(const QnNetworkResourceList& list, qint64 startTimeUSec, qint64 endTimeUSec, qint64 detail, const QList<QRegion>& motionRegions)
{
    QnRequestParamList result;

    foreach(QnNetworkResourcePtr netResource, list)
        result << QnRequestParam("mac", netResource->getMAC().toString());
    result << QnRequestParam("startTime", QString::number(startTimeUSec));
    result << QnRequestParam("endTime", QString::number(endTimeUSec));
    result << QnRequestParam("detail", QString::number(detail));
    result << QnRequestParam("format", "bin");

    QString regionStr = serializeRegionList(motionRegions);
    if (!regionStr.isEmpty())
        result << QnRequestParam("motionRegions", regionStr);

    return result;
}

QnTimePeriodList QnVideoServerConnection::recordedTimePeriods(const QnNetworkResourceList& list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, const QList<QRegion>& motionRegions)
{
    QnTimePeriodList result;
    QByteArray errorString;
    int status = recordedTimePeriods(createParamList(list, startTimeMs, endTimeMs, detail, motionRegions), result, errorString);
    if (status)
    {
        qDebug() << errorString;
    }

    return result;
}

int QnVideoServerConnection::asyncRecordedTimePeriods(const QnNetworkResourceList& list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, QList<QRegion> motionRegions, QObject *target, const char *slot) {
    detail::QnVideoServerConnectionReplyProcessor *processor = new detail::QnVideoServerConnectionReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QnTimePeriodList &, int)), target, slot);

    return asyncRecordedTimePeriods(createParamList(list, startTimeMs, endTimeMs, detail, motionRegions), processor, SLOT(at_replyReceived(int, const QnTimePeriodList&, int)));
}

void detail::VideoServerSessionManagerReplyProcessor::at_replyReceived(int status, const QByteArray &reply, const QByteArray& /*errorString*/, int handle)
{
    QnTimePeriodList result;
    if(status == 0)
    {
        if (reply.startsWith("BIN"))
        {
            QnTimePeriod::decode((const quint8*) reply.constData()+3, reply.size()-3, result);
        }
        else {
            qWarning() << "VideoServerConnection: unexpected message received.";
            status = -1;
        }
    }

    emit finished(status, result, handle);

    deleteLater();
}

int QnVideoServerConnection::recordedTimePeriods(const QnRequestParamList& params, QnTimePeriodList& result, QByteArray& errorString)
{
    QByteArray reply;

    if(SessionManager::instance()->sendGetRequest(m_url, "RecordedTimePeriods", params, reply, errorString))
        return 1;

    if (reply.startsWith("BIN"))
    {
        QnTimePeriod::decode((const quint8*) reply.constData()+3, reply.size()-3, result);
    } else {
        qWarning() << "VideoServerConnection: unexpected message received.";
        return -1;
    }

    return 0;
}

int QnVideoServerConnection::asyncRecordedTimePeriods(const QnRequestParamList& params, QObject *target, const char *slot)
{
    detail::VideoServerSessionManagerReplyProcessor *processor = new detail::VideoServerSessionManagerReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QnTimePeriodList&, int)), target, slot);

    return SessionManager::instance()->sendAsyncGetRequest(m_url, "RecordedTimePeriods", params, processor, SLOT(at_replyReceived(int, QByteArray, QByteArray, int)));
}

void QnVideoServerConnection::setProxyAddr(const QString& addr, int port)
{
    m_proxyAddr = addr;
    m_proxyPort = port;
    //QMyNetworkProxyFactory::instance()->clearProxyList();

    /*
    QNetworkProxy proxy;
    proxy.setType(port ? QNetworkProxy::HttpProxy : QNetworkProxy::NoProxy);
    proxy.setHostName(addr);
    proxy.setPort(port);
    proxy.setApplicationProxy(proxy);
    */

    //proxy.setUser("username");
    //proxy.setPassword("password");

    //QNetworkProxyQuery proxyQuery("http://?/RecordedTimePeriods")
}

