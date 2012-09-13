#include <QDebug>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QSharedPointer>
#include "utils/common/util.h"

#include "video_server_connection.h"
#include "video_server_connection_p.h"
#include "session_manager.h"

#include <api/serializer/serializer.h>
#include <api/video_server_statistics_data.h>

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
        if (QnVideoServerConnection::getProxyPort() == 0 || query.url().path().isEmpty() || query.url().path() == QLatin1String("api/ping/")) {
            rez << QNetworkProxy(QNetworkProxy::NoProxy);
            return rez;
        }
        QString host = query.peerHostName();
        QUrl url = query.url();
        url.setPath(QString());
        url.setUserInfo(QString());
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


namespace detail
{
    ////////////////////////////////////////////////////////////////
    // VideoServerGetParamReplyProcessor
    ////////////////////////////////////////////////////////////////
    const QList< QPair< QString, QVariant> >& VideoServerGetParamReplyProcessor::receivedParams() const
    {
        return m_receivedParams;
    }

    //!Parses response mesasge body and fills \a m_receivedParams
    void VideoServerGetParamReplyProcessor::parseResponse( const QByteArray& responseMessageBody )
    {
        m_receivedParams.clear();

        const QList<QByteArray>& paramPairs = responseMessageBody.split( '\n' );
        for( QList<QByteArray>::const_iterator
            it = paramPairs.begin();
            it != paramPairs.end();
            ++it )
        {
            int sepPos = it->indexOf( '=' );
            if( sepPos == -1 )   //no param value
                m_receivedParams.push_back( qMakePair( QString::fromUtf8(it->data(), it->size()), QVariant() ) );
            else
            {
                const QByteArray& paramName = it->mid( 0, sepPos );
                m_receivedParams.push_back( qMakePair( QString::fromUtf8(paramName.data(), paramName.size()), QVariant(it->mid( sepPos+1 )) ) );
            }
        }
    }

    void VideoServerGetParamReplyProcessor::at_replyReceived( int status, const QByteArray& reply, const QByteArray /* &errorString */ , int /*handle*/ )
    {
        parseResponse( reply );
        emit finished( status, m_receivedParams );
        deleteLater();
    }


    ////////////////////////////////////////////////////////////////
    // VideoServerSetParamReplyProcessor
    ////////////////////////////////////////////////////////////////
    //!QList<QPair<paramName, operation result> >. Return value is actual only after response has been handled
    const QList<QPair<QString, bool> >& VideoServerSetParamReplyProcessor::operationResult() const
    {
        return m_operationResult;
    }

    //!Parses response mesasge body and fills \a m_receivedParams
    void VideoServerSetParamReplyProcessor::parseResponse( const QByteArray& responseMessageBody )
    {
        m_operationResult.clear();
        const QList<QByteArray>& paramPairs = responseMessageBody.split( '\n' );
        for( QList<QByteArray>::const_iterator
            it = paramPairs.begin();
            it != paramPairs.end();
            ++it )
        {
            int sepPos = it->indexOf( ':' );
            if( sepPos == -1 )   //wrongFormat
                continue;

            const QByteArray& paramName = it->mid( sepPos+1 );
            m_operationResult.push_back( qMakePair(
                QString::fromUtf8(paramName.data(), paramName.size()),
                it->mid( 0, sepPos ) == "ok" ) );
        }
    }

    void VideoServerSetParamReplyProcessor::at_replyReceived(int status, const QByteArray& reply, const QByteArray /* &errorString */ , int /*handle*/)
    {
        parseResponse(reply);
        emit finished(status, m_operationResult);
        deleteLater();
    }

    void VideoServerSimpleReplyProcessor::at_replyReceived(int status, const QByteArray &reply, const QByteArray &errorString, int handle) {
        Q_UNUSED(reply)
        Q_UNUSED(errorString)
        emit finished(status, handle);
    }

} // namespace detail

// ---------------------------------- QnVideoServerConnection ---------------------

QnVideoServerConnection::QnVideoServerConnection(const QUrl &url, QObject *parent):
    QObject(parent),
    m_url(url)
{
    QnNetworkProxyFactory::instance()->addToProxyList(m_url);
}

QnVideoServerConnection::~QnVideoServerConnection() {}

QnRequestParamList QnVideoServerConnection::createParamList(const QnNetworkResourceList &list, qint64 startTimeUSec, qint64 endTimeUSec, qint64 detail, const QList<QRegion>& motionRegions)
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

QnTimePeriodList QnVideoServerConnection::recordedTimePeriods(const QnNetworkResourceList &list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, const QList<QRegion> &motionRegions)
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

int QnVideoServerConnection::asyncRecordedTimePeriods(const QnNetworkResourceList &list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, const QList<QRegion> &motionRegions, QObject *target, const char *slot) 
{
    detail::QnVideoServerConnectionReplyProcessor *processor = new detail::QnVideoServerConnectionReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QnTimePeriodList &, int)), target, slot);

    return asyncRecordedTimePeriods(createParamList(list, startTimeMs, endTimeMs, detail, motionRegions), processor, SLOT(at_replyReceived(int, const QnTimePeriodList&, int)));
}

int QnVideoServerConnection::asyncGetParamList(const QnNetworkResourcePtr &camera, const QStringList &params, QObject *target, const char *slot)
{
    detail::VideoServerGetParamReplyProcessor* processor = new detail::VideoServerGetParamReplyProcessor();
    connect(
        processor,
        SIGNAL(finished( int, const QList< QPair< QString, QVariant> >& )),
        target,
        slot,
        Qt::QueuedConnection);

    QnRequestParamList requestParams;
    requestParams << QnRequestParam( "res_id", camera->getPhysicalId() );
    foreach( QString param, params )
    {
        requestParams << QnRequestParam( param, QString() );
    }
    return QnSessionManager::instance()->sendAsyncGetRequest(
        m_url,
        QLatin1String("getCameraParam"),
        requestParams,
        processor,
        SLOT(at_replyReceived(int, QByteArray, QByteArray, int)));
}

int QnVideoServerConnection::getParamList(
    const QnNetworkResourcePtr &camera,
    const QStringList &params,
    QList< QPair< QString, QVariant> > *paramValues)
{
    QByteArray reply;
    QByteArray errorString;
    QnRequestParamList requestParams;
    requestParams << QnRequestParam( "res_id", camera->getPhysicalId() );
    foreach( QString param, params )
        requestParams << QnRequestParam( param, QString() );
    int status = QnSessionManager::instance()->sendGetRequest( m_url, QLatin1String("getCameraParam"), requestParams, reply, errorString );

    detail::VideoServerGetParamReplyProcessor processor;
    processor.parseResponse( reply );
    *paramValues = processor.receivedParams();

    return status;
}

int QnVideoServerConnection::asyncSetParam(const QnNetworkResourcePtr &camera, const QList<QPair<QString, QVariant> > &params, QObject *target, const char *slot) 
{
    detail::VideoServerSetParamReplyProcessor* processor = new detail::VideoServerSetParamReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QList<QPair<QString, bool> > &)), target, slot, Qt::QueuedConnection);

    QnRequestParamList requestParams;
    requestParams << QnRequestParam("res_id", camera->getPhysicalId());
    for( QList< QPair< QString, QVariant> >::const_iterator it = params.begin(); it != params.end(); ++it) 
        requestParams << QnRequestParam(it->first, it->second.toString());

    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("setCameraParam"), requestParams, processor, SLOT(at_replyReceived(int, QByteArray, QByteArray, int)));
}

int QnVideoServerConnection::setParamList(const QnNetworkResourcePtr &camera, const QList<QPair<QString, QVariant> > &params, QList<QPair<QString, bool> > *operationResult)
{
    QByteArray reply;
    QByteArray errorString;
    QnRequestParamList requestParams;
    requestParams << QnRequestParam( "res_id", camera->getPhysicalId() );
    for( QList< QPair< QString, QVariant> >::const_iterator it = params.begin(); it != params.end(); ++it)
        requestParams << QnRequestParam( it->first, it->second.toString() );

    int status = QnSessionManager::instance()->sendGetRequest( m_url, QLatin1String("setCameraParam"), requestParams, reply, errorString );

    detail::VideoServerSetParamReplyProcessor processor;
    processor.parseResponse( reply );
    *operationResult = processor.operationResult();

    return status;
}

int QnVideoServerConnection::asyncGetFreeSpace(const QString &path, QObject *target, const char *slot)
{
    detail::VideoServerSessionManagerFreeSpaceRequestReplyProcessor *processor = new detail::VideoServerSessionManagerFreeSpaceRequestReplyProcessor();
    connect(processor, SIGNAL(finished(int, qint64, qint64, int)), target, slot);

    QnRequestParamList params;
    params << QnRequestParam("path", path);
    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("GetFreeSpace"), params, processor, SLOT(at_replyReceived(int, QByteArray, QByteArray, int)));
}

int QnVideoServerConnection::asyncGetStatistics(QObject *target, const char *slot){
    detail::VideoServerSessionManagerStatisticsRequestReplyProcessor *processor = new detail::VideoServerSessionManagerStatisticsRequestReplyProcessor();
    connect(processor, SIGNAL(finished(const QnStatisticsDataList &/* data */)), target, slot, Qt::QueuedConnection);
    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("statistics"), QnRequestParamList(), processor, SLOT(at_replyReceived(int, QByteArray, QByteArray, int)));
}

int QnVideoServerConnection::syncGetStatistics(QObject *target, const char *slot) {
    QByteArray reply;
    QByteArray errorString;
    int status = QnSessionManager::instance()->sendGetRequest(m_url, QLatin1String("statistics"), reply, errorString);
    
    detail::VideoServerSessionManagerStatisticsRequestReplyProcessor *processor = new detail::VideoServerSessionManagerStatisticsRequestReplyProcessor();
    connect(processor, SIGNAL(finished(int)), target, slot, Qt::DirectConnection);
    processor->at_replyReceived(status, reply, errorString, 0);
    
    return status;
}

int QnVideoServerConnection::asyncGetManualCameraSearch(QObject *target, const char *slot,
                                                    const QString &startAddr, const QString &endAddr, const QString& username, const QString &password, const int port){

    QnRequestParamList params;
    params << QnRequestParam("start_ip", startAddr);
    params << QnRequestParam("end_ip", endAddr);
    params << QnRequestParam("user", username);
    params << QnRequestParam("password", password);
    params << QnRequestParam("port" ,QString::number(port));

    detail::VideoServerSessionManagerSearchCamerasRequestReplyProcessor *processor = new detail::VideoServerSessionManagerSearchCamerasRequestReplyProcessor();
    connect(processor, SIGNAL(finished(const QnCamerasFoundInfoList &)), target, slot, Qt::QueuedConnection);
    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("manualCamera/search"), params, processor, SLOT(at_replyReceived(int, QByteArray, QByteArray, int)));
}

void detail::VideoServerSessionManagerReplyProcessor::at_replyReceived(int status, const QByteArray &reply, const QByteArray &/*errorString*/, int handle) 
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
QByteArray extractXmlBody(const QByteArray &body, const QByteArray &tagName, int *from = NULL)
{
    QByteArray tagStart = QByteArray("<") + tagName + QByteArray(">");
    int bodyStart = body.indexOf(tagStart, from ? *from : 0);
    if (bodyStart >= 0)
        bodyStart += tagStart.length();
    QByteArray tagEnd = QByteArray("</") + tagName + QByteArray(">");
    int bodyEnd = body.indexOf(tagEnd, bodyStart);
    if (bodyStart >= 0 && bodyEnd >= 0){
        if (from)
            *from = bodyEnd + tagEnd.length();
        return body.mid(bodyStart, bodyEnd - bodyStart).trimmed();
    }
    else
        return QByteArray();
}

void detail::VideoServerSessionManagerFreeSpaceRequestReplyProcessor::at_replyReceived(int status, const QByteArray &reply, const QByteArray &/*errorString*/, int handle)
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

int QnVideoServerConnection::recordedTimePeriods(const QnRequestParamList &params, QnTimePeriodList &result, QByteArray &errorString)
{
    QByteArray reply;

    if(QnSessionManager::instance()->sendGetRequest(m_url, QLatin1String("RecordedTimePeriods"), params, reply, errorString))
        return 1;

    if (reply.startsWith("BIN")) {
        QnTimePeriod::decode((const quint8*) reply.constData()+3, reply.size()-3, result);
    } else {
        qWarning() << "VideoServerConnection: unexpected message received.";
        return -1;
    }

    return 0;
}

int QnVideoServerConnection::asyncRecordedTimePeriods(const QnRequestParamList &params, QObject *target, const char *slot)
{
    detail::VideoServerSessionManagerReplyProcessor *processor = new detail::VideoServerSessionManagerReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QnTimePeriodList&, int)), target, slot);

    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("RecordedTimePeriods"), params, processor, SLOT(at_replyReceived(int, QByteArray, QByteArray, int)));
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

void detail::VideoServerSessionManagerStatisticsRequestReplyProcessor::at_replyReceived(int status, const QByteArray &reply, const QByteArray &, int) {
    if(status == 0)
    {
        //QByteArray usagestr = extractXmlBody(reply, "usage"); // usage of the current process
        QByteArray usageStr = extractXmlBody(reply, "load"); // total cpu load of the current machine
        int usage =  usageStr.toShort();
        QByteArray modelStr = extractXmlBody(reply, "model");

        QnStatisticsDataList data;
        data.append(QnStatisticsDataItem(QLatin1String(modelStr), usage, QnStatisticsDataItem::CPU));

        QByteArray storages = extractXmlBody(reply, "storages");
        QByteArray storage;
        int from = 0;
        do 
        {
            storage = extractXmlBody(storages, "storage", &from);
            if (storage.length() == 0)
                break;
            QString url = QLatin1String(extractXmlBody(storage, "url"));
            usage = extractXmlBody(storage, "usage").toShort();
            data.append(QnStatisticsDataItem(url, usage, QnStatisticsDataItem::HDD));
        } while (storage.length() > 0);
        emit finished(data); 
    }
    deleteLater();
}

void detail::VideoServerSessionManagerSearchCamerasRequestReplyProcessor::at_replyReceived(int status, const QByteArray &reply, const QByteArray &errorString, int handle ){
    Q_UNUSED(errorString)
    Q_UNUSED(handle)

    QnCamerasFoundInfoList result;
    if (status == 0){
        QByteArray root = extractXmlBody(reply, "reply");
        QByteArray resource;
        int from = 0;
        do
        {
            resource = extractXmlBody(root, "resource", &from);
            if (resource.length() == 0)
                break;
            QString url = QLatin1String(extractXmlBody(resource, "url"));
            QString name = QLatin1String(extractXmlBody(resource, "name"));
            QString manufacture = QLatin1String(extractXmlBody(resource, "manufacture"));
            result.append(QnCamerasFoundInfo(url, name, manufacture));

        } while (resource.length() > 0);
    }

    emit finished(result);
    deleteLater();
}

int QnVideoServerConnection::asyncPtzMove(const QnNetworkResourcePtr &camera, qreal xSpeed, qreal ySpeed, qreal zoomSpeed, QObject *target, const char *slot) {
    detail::VideoServerSimpleReplyProcessor* processor = new detail::VideoServerSimpleReplyProcessor();
    connect(processor, SIGNAL(finished(int, int)), target, slot, Qt::QueuedConnection);

    QnRequestParamList requestParams;
    requestParams << QnRequestParam("res_id", camera->getPhysicalId());
    requestParams << QnRequestParam("xSpeed", QString::number(xSpeed));
    requestParams << QnRequestParam("ySpeed", QString::number(ySpeed));
    requestParams << QnRequestParam("zoomSpeed", QString::number(zoomSpeed));

    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("ptz/move"), requestParams, processor, SLOT(at_replyReceived(int, QByteArray, QByteArray, int)));
}

int QnVideoServerConnection::asyncPtzStop(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    detail::VideoServerSimpleReplyProcessor* processor = new detail::VideoServerSimpleReplyProcessor();
    connect(processor, SIGNAL(finished(int, int)), target, slot, Qt::QueuedConnection);

    QnRequestParamList requestParams;
    requestParams << QnRequestParam("res_id", camera->getPhysicalId());

    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("ptz/stop"), requestParams, processor, SLOT(at_replyReceived(int, QByteArray, QByteArray, int)));
}


