#include <QDebug>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QSharedPointer>
#include "utils/common/util.h"

#include "video_server_connection.h"
#include "video_server_connection_p.h"
#include "session_manager.h"
#include "api/serializer/serializer.h"

QString QnVideoServerConnection::m_proxyAddr;
int QnVideoServerConnection::m_proxyPort = 0;

// -------------------------------------------------------------------------- //
// QnNetworkProxyFactory
// -------------------------------------------------------------------------- //
class QnNetworkProxyFactory: public QObject, public QNetworkProxyFactory {
public:
    QnNetworkProxyFactory()
    {
    }
    
    virtual ~QnNetworkProxyFactory()
    {
    }

    void addToProxyList(const QUrl& url)
    {
        m_proxyInfo.insert(url);
    }

    void clearProxyList()
    {
        m_proxyInfo.clear();
    }

    static QnNetworkProxyFactory* instance();

protected:
    virtual QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query = QNetworkProxyQuery()) override
    {
        QList<QNetworkProxy> rez;
        if (QnVideoServerConnection::getProxyPort() == 0 || query.url().queryItems().isEmpty()) {
            rez << QNetworkProxy(QNetworkProxy::NoProxy);
            return rez;
        }
        QString host = query.peerHostName();
        QUrl url = query.url();
        url.setPath("");
        url.setUserInfo("");
        url.setQueryItems(QList<QPair<QString, QString> >());
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

Q_GLOBAL_STATIC(QnNetworkProxyFactory, qn_reserveProxyFactory);

Q_GLOBAL_STATIC_WITH_INITIALIZER(QWeakPointer<QnNetworkProxyFactory>, qn_globalProxyFactory, {
    QnNetworkProxyFactory *instance = new QnNetworkProxyFactory();
    *x = instance;

    /* Qt will take ownership of the supplied instance. */
    QNetworkProxyFactory::setApplicationProxyFactory(instance);
});

QnNetworkProxyFactory *QnNetworkProxyFactory::instance()
{
    QWeakPointer<QnNetworkProxyFactory> *result = qn_globalProxyFactory();
    if(*result) {
        return result->data();
    } else {
        return qn_reserveProxyFactory();
    }
}


namespace {
    // for video server methods
    QnTimePeriodList parseBinaryTimePeriods(const QByteArray &reply)
    {
        QnTimePeriodList result;

        QByteArray localReply = reply;
        QBuffer buffer(&localReply);
        buffer.open(QIODevice::ReadOnly);

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
    QnNetworkProxyFactory::instance()->addToProxyList(m_url);
}

QnVideoServerConnection::~QnVideoServerConnection() {}

QnRequestParamList QnVideoServerConnection::createParamList(const QnNetworkResourceList& list, qint64 startTimeUSec, qint64 endTimeUSec, qint64 detail, const QList<QRegion>& motionRegions)
{
    QnRequestParamList result;

    foreach(QnNetworkResourcePtr netResource, list)
        result << QnRequestParam("physicalId", netResource->getPhysicalId());
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

int QnVideoServerConnection::asyncRecordedTimePeriods(const QnNetworkResourceList& list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, QList<QRegion> motionRegions, QObject *target, const char *slot) 
{
    detail::QnVideoServerConnectionReplyProcessor *processor = new detail::QnVideoServerConnectionReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QnTimePeriodList &, int)), target, slot);

    return asyncRecordedTimePeriods(createParamList(list, startTimeMs, endTimeMs, detail, motionRegions), processor, SLOT(at_replyReceived(int, const QnTimePeriodList&, int)));
}

int QnVideoServerConnection::asyncGetFreeSpace(const QString& path, QObject *target, const char *slot)
{
    detail::VideoServerSessionManagerFreeSpaceRequestReplyProcessor *processor = new detail::VideoServerSessionManagerFreeSpaceRequestReplyProcessor();
    connect(processor, SIGNAL(finished(int, qint64, qint64, int)), target, slot);

    QnRequestParamList params;
    params << QnRequestParam("path", path);
    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, "GetFreeSpace", params, processor, SLOT(at_replyReceived(int, QByteArray, QByteArray, int)));

}

int QnVideoServerConnection::asyncGetStatistics(QObject *target, const char *slot){
    detail::VideoServerSessionManagerStatisticsRequestReplyProcessor *processor = new detail::VideoServerSessionManagerStatisticsRequestReplyProcessor();
    connect(processor, SIGNAL(finished(const QByteArray &)), target, slot, Qt::DirectConnection);
    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, "api/statistics", processor, SLOT(at_replyReceived(int, QByteArray, QByteArray, int)));
}

int QnVideoServerConnection::syncGetStatistics(QObject *target, const char *slot){
    QByteArray reply;
    QByteArray errorString;
    int status = QnSessionManager::instance()->sendGetRequest(m_url, "api/statistics", reply, errorString);

    detail::VideoServerSessionManagerStatisticsRequestReplyProcessor *processor = new detail::VideoServerSessionManagerStatisticsRequestReplyProcessor();
    connect(processor, SIGNAL(finished(int)), target, slot, Qt::DirectConnection);
    processor->at_replyReceived(status, reply, errorString, 0);

    return status;
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

// very simple parser. Used for parsing own created XML
QByteArray extractXmlBody(const QByteArray& body, const QByteArray tagName)
{
	QByteArray tagStart = QByteArray("<") + tagName + QByteArray(">");
	int bodyStart = body.indexOf(tagStart);
	if (bodyStart >= 0)
		bodyStart += tagStart.length();
	QByteArray tagEnd = QByteArray("</") + tagName + QByteArray(">");
	int bodyEnd = body.indexOf(tagEnd);
	if (bodyStart >= 0 && bodyEnd >= 0)
		return body.mid(bodyStart, bodyEnd - bodyStart).trimmed();
	else
		return QByteArray();
}

void detail::VideoServerSessionManagerFreeSpaceRequestReplyProcessor::at_replyReceived(int status, const QByteArray &reply, const QByteArray& /*errorString*/, int handle)
{
    qint64 freeSpace = -1;
	qint64 usedSpace = -1;

    if(status == 0)
    {
		QByteArray message = extractXmlBody(reply, "root");
		freeSpace = extractXmlBody(message, "freeSpace").toLongLong();
		usedSpace = extractXmlBody(message, "usedSpace").toLongLong();
    }

    emit finished(status, freeSpace, usedSpace, handle);

    deleteLater();
}

int QnVideoServerConnection::recordedTimePeriods(const QnRequestParamList& params, QnTimePeriodList& result, QByteArray& errorString)
{
    QByteArray reply;

    if(QnSessionManager::instance()->sendGetRequest(m_url, "RecordedTimePeriods", params, reply, errorString))
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

    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, "RecordedTimePeriods", params, processor, SLOT(at_replyReceived(int, QByteArray, QByteArray, int)));
}

void QnVideoServerConnection::setProxyAddr(const QString& addr, int port)
{
    m_proxyAddr = addr;
    m_proxyPort = port;
    //QnNetworkProxyFactory::instance()->clearProxyList();

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

void detail::VideoServerSessionManagerStatisticsRequestReplyProcessor::at_replyReceived(int status, const QByteArray &reply, const QByteArray ,int ){
    if(status == 0)
    {
        QByteArray root = extractXmlBody(reply, "root");
        QByteArray misc = extractXmlBody(root, "misc");
        int usage = qrand() % 101;
        qDebug() << "cpuInfo" << usage;
        emit finished(usage); 
    }
    deleteLater();
}