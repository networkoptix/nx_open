#include <QDebug>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QSharedPointer>

#include "utils/common/util.h"
#include "utils/common/request_param.h"
#include "utils/common/space_mapper.h"
#include "utils/common/json.h"

#include "media_server_connection.h"
#include "media_server_connection_p.h"
#include "session_manager.h"

#include <api/serializer/serializer.h>
#include <api/media_server_statistics_data.h>

// -------------------------------------------------------------------------- //
// QnNetworkProxyFactory
// -------------------------------------------------------------------------- //
/**
 * Note that instance of this class will be used from several threads, and
 * must therefore be thread-safe.
 */
class QnNetworkProxyFactory: public QObject, public QNetworkProxyFactory {
public:
    QnNetworkProxyFactory()
    {
    }
    
    virtual ~QnNetworkProxyFactory()
    {
    }

    void removeFromProxyList(const QUrl& url)
    {
        QMutexLocker locker(&m_mutex);

        m_proxyInfo.remove(url);
    }

    void addToProxyList(const QUrl& url, const QString& addr, int port)
    {
        QMutexLocker locker(&m_mutex);

        m_proxyInfo.insert(url, ProxyInfo(addr, port));
    }

    void clearProxyList()
    {
        QMutexLocker locker(&m_mutex);

        m_proxyInfo.clear();
    }

    static QnNetworkProxyFactory* instance();

protected:
    virtual QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query = QNetworkProxyQuery()) override
    {
        QList<QNetworkProxy> rez;
        if (query.url().path().isEmpty() || query.url().path() == QLatin1String("api/ping/")) {
            rez << QNetworkProxy(QNetworkProxy::NoProxy);
            return rez;
        }
        QString host = query.peerHostName();
        QUrl url = query.url();
        url.setPath(QString());
        url.setUserInfo(QString());
        url.setQueryItems(QList<QPair<QString, QString> >());

        QMutexLocker locker(&m_mutex);
        QMap<QUrl, ProxyInfo>::const_iterator itr = m_proxyInfo.find(url);
        if (itr == m_proxyInfo.end())
            rez << QNetworkProxy(QNetworkProxy::NoProxy);
        else 
            rez << QNetworkProxy(QNetworkProxy::HttpProxy, itr.value().addr, itr.value().port);
        return rez;
    }

private:
    struct ProxyInfo {
        ProxyInfo(): port(0) {}
        ProxyInfo(const QString& _addr, int _port): addr(_addr), port(_port) {}
        QString addr;
        int port;
    };
    QMutex m_mutex;
    QMap<QUrl, ProxyInfo> m_proxyInfo;
};

Q_GLOBAL_STATIC(QnNetworkProxyFactory, qn_reserveProxyFactory);

QWeakPointer<QnNetworkProxyFactory> createGlobalProxyFactory() {
    QnNetworkProxyFactory *result(new QnNetworkProxyFactory());

    /* Qt will take ownership of the supplied instance. */
    QNetworkProxyFactory::setApplicationProxyFactory(result); // TODO: #Elric we have a race if this code is run several times from different threads.
    
    return result;
}

Q_GLOBAL_STATIC_WITH_ARGS(QWeakPointer<QnNetworkProxyFactory>, qn_globalProxyFactory, (createGlobalProxyFactory()));

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

void detail::QnMediaServerConnectionReplyProcessor::at_replyReceived(int status, const QnTimePeriodList &result, int handle)
{
    emit finished(status, result, handle);
    deleteLater();
}


namespace detail
{
    ////////////////////////////////////////////////////////////////
    // QnMediaServerGetParamReplyProcessor
    ////////////////////////////////////////////////////////////////
    const QList< QPair< QString, QVariant> >& QnMediaServerGetParamReplyProcessor::receivedParams() const
    {
        return m_receivedParams;
    }

    //!Parses response mesasge body and fills \a m_receivedParams
    void QnMediaServerGetParamReplyProcessor::parseResponse( const QByteArray& responseMessageBody )
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

    void QnMediaServerGetParamReplyProcessor::at_replyReceived(const QnHTTPRawResponse& response, int /*handle*/ )
    {
        parseResponse(response.data);
        emit finished(response.status, m_receivedParams);
        deleteLater();
    }


    ////////////////////////////////////////////////////////////////
    // QnMediaServerSetParamReplyProcessor
    ////////////////////////////////////////////////////////////////
    //!QList<QPair<paramName, operation result> >. Return value is actual only after response has been handled
    const QList<QPair<QString, bool> >& QnMediaServerSetParamReplyProcessor::operationResult() const
    {
        return m_operationResult;
    }

    //!Parses response mesasge body and fills \a m_receivedParams
    void QnMediaServerSetParamReplyProcessor::parseResponse( const QByteArray& responseMessageBody )
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

    void QnMediaServerSetParamReplyProcessor::at_replyReceived(const QnHTTPRawResponse& response, int /*handle*/)
    {
        parseResponse(response.data);
        emit finished(response.status, m_operationResult);
        deleteLater();
    }

    void QnMediaServerSimpleReplyProcessor::at_replyReceived(const QnHTTPRawResponse& response, int handle) {
        emit finished(response.status, handle);
    }

} // namespace detail

// ---------------------------------- QnMediaServerConnection ---------------------

QnMediaServerConnection::QnMediaServerConnection(QnResourcePtr mServer, QObject *parent):
    QObject(parent),
    m_mServer(mServer),
    m_proxyPort(0)
{
    m_url = m_mServer.dynamicCast<QnMediaServerResource>()->getApiUrl();
}

QnMediaServerConnection::~QnMediaServerConnection() {}

QnRequestParamList QnMediaServerConnection::createParamList(const QnNetworkResourceList &list, qint64 startTimeUSec, qint64 endTimeUSec, qint64 detail, const QList<QRegion>& motionRegions)
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

QnTimePeriodList QnMediaServerConnection::recordedTimePeriods(const QnNetworkResourceList &list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, const QList<QRegion> &motionRegions)
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

int QnMediaServerConnection::asyncRecordedTimePeriods(const QnNetworkResourceList &list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, const QList<QRegion> &motionRegions, QObject *target, const char *slot) 
{
    detail::QnMediaServerConnectionReplyProcessor *processor = new detail::QnMediaServerConnectionReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QnTimePeriodList &, int)), target, slot);

    return asyncRecordedTimePeriods(createParamList(list, startTimeMs, endTimeMs, detail, motionRegions), processor, SLOT(at_replyReceived(int, const QnTimePeriodList&, int)));
}

int QnMediaServerConnection::asyncGetParamList(const QnNetworkResourcePtr &camera, const QStringList &params, QObject *target, const char *slot)
{
    detail::QnMediaServerGetParamReplyProcessor* processor = new detail::QnMediaServerGetParamReplyProcessor();
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
        QnRequestHeaderList(),
        requestParams,
        processor,
        SLOT(at_replyReceived(QnHTTPRawResponse, int)));
}

int QnMediaServerConnection::getParamList(
    const QnNetworkResourcePtr &camera,
    const QStringList &params,
    QList< QPair< QString, QVariant> > *paramValues)
{
    QnHTTPRawResponse response;

    QnRequestParamList requestParams;
    requestParams << QnRequestParam( "res_id", camera->getPhysicalId() );
    foreach( QString param, params )
        requestParams << QnRequestParam( param, QString() );
    int status = QnSessionManager::instance()->sendGetRequest( m_url, QLatin1String("getCameraParam"), QnRequestHeaderList(), requestParams,
        response );

    detail::QnMediaServerGetParamReplyProcessor processor;
    processor.parseResponse( response.data );
    *paramValues = processor.receivedParams();

    return status;
}

int QnMediaServerConnection::asyncSetParam(const QnNetworkResourcePtr &camera, const QList<QPair<QString, QVariant> > &params, QObject *target, const char *slot) 
{
    detail::QnMediaServerSetParamReplyProcessor* processor = new detail::QnMediaServerSetParamReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QList<QPair<QString, bool> > &)), target, slot, Qt::QueuedConnection);

    QnRequestParamList requestParams;
    requestParams << QnRequestParam("res_id", camera->getPhysicalId());
    for( QList< QPair< QString, QVariant> >::const_iterator it = params.begin(); it != params.end(); ++it) 
        requestParams << QnRequestParam(it->first, it->second.toString());

    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("setCameraParam"), QnRequestHeaderList(), requestParams, processor, SLOT(at_replyReceived(QnHTTPRawResponse, int)));
}

int QnMediaServerConnection::setParamList(const QnNetworkResourcePtr &camera, const QList<QPair<QString, QVariant> > &params, QList<QPair<QString, bool> > *operationResult)
{
    QnHTTPRawResponse response;
    QnRequestParamList requestParams;

    requestParams << QnRequestParam( "res_id", camera->getPhysicalId() );
    for( QList< QPair< QString, QVariant> >::const_iterator it = params.begin(); it != params.end(); ++it)
        requestParams << QnRequestParam( it->first, it->second.toString() );

    int status = QnSessionManager::instance()->sendGetRequest( m_url, QLatin1String("setCameraParam"), QnRequestHeaderList(), requestParams,
        response );

    detail::QnMediaServerSetParamReplyProcessor processor;
    processor.parseResponse( response.data );
    *operationResult = processor.operationResult();

    return status;
}

int QnMediaServerConnection::asyncGetFreeSpace(const QString &path, QObject *target, const char *slot)
{
    detail::QnMediaServerFreeSpaceReplyProcessor *processor = new detail::QnMediaServerFreeSpaceReplyProcessor();
    connect(processor, SIGNAL(finished(int, qint64, qint64, int)), target, slot);

    QnRequestParamList params;
    params << QnRequestParam("path", path);
    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("GetFreeSpace"), QnRequestHeaderList(), params, processor, SLOT(at_replyReceived(QnHTTPRawResponse, int)));
}

int QnMediaServerConnection::asyncGetStatistics(QObject *target, const char *slot){
    detail::QnMediaServerStatisticsReplyProcessor *processor = new detail::QnMediaServerStatisticsReplyProcessor();
    connect(processor, SIGNAL(finished(const QnStatisticsDataList &/* data */)), target, slot, Qt::QueuedConnection);
    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("statistics"), QnRequestHeaderList(), QnRequestParamList(), processor, SLOT(at_replyReceived(QnHTTPRawResponse, int)));
}

int QnMediaServerConnection::syncGetStatistics(QObject *target, const char *slot) {
    QnHTTPRawResponse response;
    int status = QnSessionManager::instance()->sendGetRequest(m_url, QLatin1String("statistics"), response);
    
    detail::QnMediaServerStatisticsReplyProcessor *processor = new detail::QnMediaServerStatisticsReplyProcessor();
    connect(processor, SIGNAL(finished(int)), target, slot, Qt::DirectConnection);
    processor->at_replyReceived(response, 0);
    
    return status;
}

int QnMediaServerConnection::asyncManualCameraSearch(const QString &startAddr, const QString &endAddr, const QString& username, const QString &password, const int port,
                                                        QObject *target, const char *slotSuccess, const char *slotError){

    QnRequestParamList params;
    params << QnRequestParam("start_ip", startAddr);
    if (!endAddr.isEmpty())
        params << QnRequestParam("end_ip", endAddr);
    params << QnRequestParam("user", username);
    params << QnRequestParam("password", password);
    params << QnRequestParam("port" ,QString::number(port));

    detail::QnMediaServerManualCameraReplyProcessor *processor = new detail::QnMediaServerManualCameraReplyProcessor();
    connect(processor, SIGNAL(finishedSearch(const QnCamerasFoundInfoList &)), target, slotSuccess, Qt::QueuedConnection);
    connect(processor, SIGNAL(searchError(int, const QString &)), target, slotError, Qt::QueuedConnection);
    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("manualCamera/search"), QnRequestHeaderList(), params, processor, SLOT(at_searchReplyReceived(QnHTTPRawResponse, int)));
}

int QnMediaServerConnection::asyncManualCameraAdd(const QStringList &urls, const QStringList &manufacturers, const QString &username, const QString &password,
                                                     QObject *target, const char *slot){
    QnRequestParamList params;

    for (int i = 0; i < qMin(urls.count(), manufacturers.count()); i++){
        params << QnRequestParam("url", urls[i]);
        params << QnRequestParam("manufacturer", manufacturers[i]);
    }

    params << QnRequestParam("user", username);
    params << QnRequestParam("password", password);

    detail::QnMediaServerManualCameraReplyProcessor *processor = new detail::QnMediaServerManualCameraReplyProcessor();
    connect(processor, SIGNAL(finishedAdd(int)), target, slot, Qt::QueuedConnection);
    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("manualCamera/add"), QnRequestHeaderList(), params, processor, SLOT(at_addReplyReceived(QnHTTPRawResponse, int)));
}



void detail::QnMediaServerTimePeriodsReplyProcessor::at_replyReceived(const QnHTTPRawResponse& response, int handle)
{
    int status = response.status;
    const QByteArray& reply = response.data;

    QnTimePeriodList result;
    if(status == 0)
    {
        if (reply.startsWith("BIN"))
        {
            result.decode((const quint8*) reply.constData()+3, reply.size()-3);
        }
        else {
            qWarning() << "QnMediaServerConnection: unexpected message received.";
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

void detail::QnMediaServerFreeSpaceReplyProcessor::at_replyReceived(const QnHTTPRawResponse& response, int handle)
{
    qint64 freeSpace = -1;
    qint64 totalSpace = -1;

    if(response.status == 0)
    {
        QByteArray message = extractXmlBody(response.data, "root");
        freeSpace = extractXmlBody(message, "freeSpace").toLongLong();
        totalSpace = extractXmlBody(message, "totalSpace").toLongLong();
    }

    emit finished(response.status, freeSpace, totalSpace, handle);

    deleteLater();
}

int QnMediaServerConnection::recordedTimePeriods(const QnRequestParamList &params, QnTimePeriodList &result, QByteArray &errorString)
{
    QnHTTPRawResponse response;

    if(QnSessionManager::instance()->sendGetRequest(m_url, QLatin1String("RecordedTimePeriods"), QnRequestHeaderList(), params, response)) {
        errorString = response.errorString;
        return 1;
    }

    const QByteArray& reply = response.data;
    if (reply.startsWith("BIN")) {
        result.decode((const quint8*) reply.constData()+3, reply.size()-3);
    } else {
        qWarning() << "QnMediaServerConnection: unexpected message received.";
        return -1;
    }

    return 0;
}

int QnMediaServerConnection::asyncRecordedTimePeriods(const QnRequestParamList &params, QObject *target, const char *slot)
{
    detail::QnMediaServerTimePeriodsReplyProcessor *processor = new detail::QnMediaServerTimePeriodsReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QnTimePeriodList&, int)), target, slot);

    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("RecordedTimePeriods"), QnRequestHeaderList(), params, processor, SLOT(at_replyReceived(QnHTTPRawResponse, int)));
}

void QnMediaServerConnection::setProxyAddr(const QString& addr, int port)
{
    m_proxyAddr = addr;
    m_proxyPort = port;

    QUrl apiUrl = m_mServer.dynamicCast<QnMediaServerResource>()->getApiUrl();
    if (port)
        QnNetworkProxyFactory::instance()->addToProxyList(apiUrl, addr, port);
    else
        QnNetworkProxyFactory::instance()->removeFromProxyList(apiUrl);
}

void detail::QnMediaServerStatisticsReplyProcessor::at_replyReceived(const QnHTTPRawResponse& response, int) {
    const QByteArray& reply = response.data;
    int status = response.status;

    if(status == 0) {
        QnStatisticsDataList data;

        QByteArray cpuBlock = extractXmlBody(reply, "cpuinfo");
        data.append(QnStatisticsDataItem(
            QLatin1String("CPU"), 
            extractXmlBody(cpuBlock, "load").toDouble(), 
            CPU
        ));

        QByteArray memoryBlock = extractXmlBody(reply, "memory");
        data.append(QnStatisticsDataItem(
            QLatin1String("RAM"), 
            extractXmlBody(memoryBlock, "usage").toDouble(), 
            RAM
        ));

        QByteArray storagesBlock = extractXmlBody(reply, "storages"), storageBlock;
        int from = 0;
        do {
            storageBlock = extractXmlBody(storagesBlock, "storage", &from);
            if (storageBlock.length() == 0)
                break;
            data.append(QnStatisticsDataItem(
                QLatin1String(extractXmlBody(storageBlock, "url")), 
                extractXmlBody(storageBlock, "usage").toDouble(), 
                HDD
            ));
        } while (storageBlock.length() > 0);

        emit finished(data); 
    }

    deleteLater();
}

void detail::QnMediaServerManualCameraReplyProcessor::at_searchReplyReceived(const QnHTTPRawResponse& response, int handle) {
    Q_UNUSED(handle)

    const QByteArray& reply = response.data;

    QnCamerasFoundInfoList result;
    if (response.status == 0) {
        QByteArray root = extractXmlBody(reply, "reply");
        QByteArray resource;
        int from = 0;
        do {
            resource = extractXmlBody(root, "resource", &from);
            if (resource.length() == 0)
                break;
            QString url = QLatin1String(extractXmlBody(resource, "url"));
            QString name = QLatin1String(extractXmlBody(resource, "name"));
            QString manufacture = QLatin1String(extractXmlBody(resource, "manufacturer"));
            result.append(QnCamerasFoundInfo(url, name, manufacture));
        } while (resource.length() > 0);
        emit finishedSearch(result);
    } else {
        QString error = QLatin1String(extractXmlBody(reply, "root"));
        emit searchError(response.status, error);
    }
    deleteLater();
}

void detail::QnMediaServerManualCameraReplyProcessor::at_addReplyReceived(const QnHTTPRawResponse &response, int handle) {
    Q_UNUSED(handle)
    emit finishedAdd(response.status);
    deleteLater();
}

int QnMediaServerConnection::asyncPtzMove(const QnNetworkResourcePtr &camera, qreal xSpeed, qreal ySpeed, qreal zoomSpeed, QObject *target, const char *slot) {
    detail::QnMediaServerSimpleReplyProcessor *processor = new detail::QnMediaServerSimpleReplyProcessor();
    connect(processor, SIGNAL(finished(int, int)), target, slot, Qt::QueuedConnection);

    QnRequestParamList requestParams;
    requestParams << QnRequestParam("res_id", camera->getPhysicalId());
    requestParams << QnRequestParam("xSpeed", QString::number(xSpeed));
    requestParams << QnRequestParam("ySpeed", QString::number(ySpeed));
    requestParams << QnRequestParam("zoomSpeed", QString::number(zoomSpeed));

    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("ptz/move"), QnRequestHeaderList(), requestParams, processor, SLOT(at_replyReceived(QnHTTPRawResponse, int)));
}

int QnMediaServerConnection::asyncPtzStop(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    detail::QnMediaServerSimpleReplyProcessor *processor = new detail::QnMediaServerSimpleReplyProcessor();
    connect(processor, SIGNAL(finished(int, int)), target, slot, Qt::QueuedConnection);

    QnRequestParamList requestParams;
    requestParams << QnRequestParam("res_id", camera->getPhysicalId());

    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("ptz/stop"), QnRequestHeaderList(), requestParams, processor, SLOT(at_replyReceived(QnHTTPRawResponse, int)));
}

int QnMediaServerConnection::asyncPtzMoveTo(const QnNetworkResourcePtr &camera, qreal xPos, qreal yPos, qreal zoomPos, QObject *target, const char *slot) {
    detail::QnMediaServerSimpleReplyProcessor *processor = new detail::QnMediaServerSimpleReplyProcessor();
    connect(processor, SIGNAL(finished(int, int)), target, slot, Qt::QueuedConnection);

    QnRequestParamList requestParams;
    requestParams << QnRequestParam("res_id", camera->getPhysicalId());
    requestParams << QnRequestParam("xPos", QString::number(xPos));
    requestParams << QnRequestParam("yPos", QString::number(yPos));
    requestParams << QnRequestParam("zoomPos", QString::number(zoomPos));

    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("ptz/moveTo"), QnRequestHeaderList(), requestParams, processor, SLOT(at_replyReceived(QnHTTPRawResponse, int)));
}

int QnMediaServerConnection::asyncPtzGetPos(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    detail::QnMediaServerPtzGetPosReplyProcessor *processor = new detail::QnMediaServerPtzGetPosReplyProcessor();
    connect(processor, SIGNAL(finished(int, qreal, qreal, qreal, int)), target, slot, Qt::QueuedConnection);

    QnRequestParamList requestParams;
    requestParams << QnRequestParam("res_id", camera->getPhysicalId());

    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("ptz/getPosition"), QnRequestHeaderList(), requestParams, processor, SLOT(at_replyReceived(QnHTTPRawResponse, int)));
}

int QnMediaServerConnection::asyncPtzGetSpaceMapper(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    detail::QnMediaServerPtzGetSpaceMapperReplyProcessor *processor = new detail::QnMediaServerPtzGetSpaceMapperReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QnPtzSpaceMapper &, int)), target, slot, Qt::QueuedConnection);

    QnRequestParamList requestParams;
    requestParams << QnRequestParam("res_id", camera->getPhysicalId());

    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("ptz/getSpaceMapper"), QnRequestHeaderList(), requestParams, processor, SLOT(at_replyReceived(QnHTTPRawResponse, int)));
}

int QnMediaServerConnection::asyncGetTime(QObject *target, const char *slot) {
    detail::QnMediaServerGetTimeReplyProcessor *processor = new detail::QnMediaServerGetTimeReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QDateTime &, int, int)), target, slot, Qt::QueuedConnection);

    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("gettime"), QnRequestHeaderList(), QnRequestParamList(), processor, SLOT(at_replyReceived(QnHTTPRawResponse, int)));
}

void detail::QnMediaServerGetTimeReplyProcessor::at_replyReceived(const QnHTTPRawResponse& response, int handle) {
    const QByteArray& reply = response.data;

    QDateTime dateTime;
    int utcOffset = 0;

    if (response.status == 0) {
        dateTime = QDateTime::fromString(QString::fromLatin1(extractXmlBody(reply, "clock")), Qt::ISODate);
        utcOffset = QString::fromLatin1(extractXmlBody(reply, "utcOffset")).toInt();
    } else {
        qnWarning("Could not get time from media server: %1.", response.errorString);
    }

    emit finished(response.status, dateTime, utcOffset, handle);
    deleteLater();
}

void detail::QnMediaServerPtzGetPosReplyProcessor::at_replyReceived(const QnHTTPRawResponse &response, int handle) {
    const QByteArray& reply = response.data;
    qreal xPos = 0, yPos = 0, zoomPos = 0;
    
    if(response.status == 0) {
        xPos = extractXmlBody(reply, "xPos").toDouble();
        yPos = extractXmlBody(reply, "yPos").toDouble();
        zoomPos = extractXmlBody(reply, "zoomPos").toDouble();
    } else {
        qnWarning("Could not get ptz position from camera: %1.", response.errorString);
    }

    emit finished(response.status, xPos, yPos, zoomPos, handle);
    deleteLater();
}

void detail::QnMediaServerPtzGetSpaceMapperReplyProcessor::at_replyReceived(const QnHTTPRawResponse &response, int handle) {
    const QByteArray& reply = response.data;
    int status = response.status;

    QnPtzSpaceMapper mapper;
    if(response.status == 0) {
        QVariantMap map;
        if(!QJson::deserialize(reply, &map) || !QJson::deserialize(map, "mapper", &mapper))
            status = 1;
    } else {
        qnWarning("Could not get ptz space mapper for camera: %1.", response.errorString);
    }

    emit finished(status, mapper, handle);
}

