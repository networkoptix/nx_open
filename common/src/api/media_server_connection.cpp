#include "media_server_connection.h"

#include <cstring> /* For std::strstr. */

#include <QDebug>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QSharedPointer>

#include "utils/common/util.h"
#include "utils/common/request_param.h"
#include "utils/math/space_mapper.h"
#include "utils/common/json.h"
#include "utils/common/enum_name_mapper.h"

#include "session_manager.h"

#include <api/serializer/serializer.h>
#include <api/media_server_statistics_data.h>

namespace {
    QN_DEFINE_NAME_MAPPED_ENUM(RequestObject, 
        ((StorageStatusObject,      "storageStatus"))
        ((StorageSpaceObject,       "storageSpace"))
        ((TimePeriodsObject,        "RecordedTimePeriods"))
        ((StatisticsObject,         "statistics"))
        ((PtzSpaceMapperObject,     "ptz/getSpaceMapper"))
        ((PtzPositionObject,        "ptz/getPosition"))
        ((PtzSetPositionObject,     "ptz/moveTo"))
        ((PtzStopObject,            "ptz/stop"))
        ((PtzMoveObject,            "ptz/move"))
    );

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


} // anonymous namespace


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




// -------------------------------------------------------------------------- //
// QnMediaServerReplyProcessor
// -------------------------------------------------------------------------- //
QnMediaServerReplyProcessor::QnMediaServerReplyProcessor(int object): 
    m_object(object) 
{}

QnMediaServerReplyProcessor::~QnMediaServerReplyProcessor() {
    return;
}

void QnMediaServerReplyProcessor::connectNotify(const char *signal) {
    if(std::strstr(signal, "QVariant")) {
        m_emitVariant = true;
    } else {
        m_emitDefault = true;
    }
}

void QnMediaServerReplyProcessor::processReply(const QnHTTPRawResponse &response, int handle) {
    switch(m_object) {
    case StorageStatusObject: {
        int status = response.status;

        QnStorageStatusReply reply;
        if(status == 0) {
            QVariantMap map;
            if(!QJson::deserialize(response.data, &map) || !QJson::deserialize(map, "reply", &reply))
                status = 1;
        } else {
            qnWarning("Could not get storage spaces.", response.errorString);
        }

        emitFinished(status, reply, handle);
        break;
    }
    case StorageSpaceObject: {
        int status = response.status;

        QnStorageSpaceReply reply;
        if(response.status == 0) {
            QVariantMap map;
            if(!QJson::deserialize(response.data, &map) || !QJson::deserialize(map, "reply", &reply))
                status = 1;
        } else {
            qnWarning("Could not get storage spaces: %1.", response.errorString);
        }

        emitFinished(status, reply, handle);
        break;
    }
    case TimePeriodsObject: {
        int status = response.status;

        QnTimePeriodList reply;
        if(status == 0) {
            if (response.data.startsWith("BIN")) {
                reply.decode((const quint8*) response.data.constData() + 3, response.data.size() - 3);
            } else {
                qWarning() << "QnMediaServerConnection: unexpected message received.";
                status = -1;
            }
        }

        emitFinished(status, reply, handle);
        break;
    }
    case StatisticsObject: {
        const QByteArray &data = response.data;
        int status = response.status;
        QnStatisticsDataList reply;

        if(status == 0) {
            QByteArray cpuBlock = extractXmlBody(data, "cpuinfo");
            reply.append(QnStatisticsDataItem(
                QLatin1String("CPU"), 
                extractXmlBody(cpuBlock, "load").toDouble(), 
                CPU
            ));

            QByteArray memoryBlock = extractXmlBody(data, "memory");
            reply.append(QnStatisticsDataItem(
                QLatin1String("RAM"), 
                extractXmlBody(memoryBlock, "usage").toDouble(), 
                RAM
            ));

            QByteArray storagesBlock = extractXmlBody(data, "storages"), storageBlock; {
                int from = 0;
                do {
                    storageBlock = extractXmlBody(storagesBlock, "storage", &from);
                    if (storageBlock.length() == 0)
                        break;
                    reply.append(QnStatisticsDataItem(
                        QLatin1String(extractXmlBody(storageBlock, "url")),
                        extractXmlBody(storageBlock, "usage").toDouble(),
                        HDD
                    ));
                } while (storageBlock.length() > 0);
            }

            QByteArray networkBlock = extractXmlBody(data, "network"), interfaceBlock; {
                int from = 0;
                do {
                    interfaceBlock = extractXmlBody(networkBlock, "interface", &from);
                    if (interfaceBlock.length() == 0)
                        break;
                    QString interfaceName = QLatin1String(extractXmlBody(interfaceBlock, "name"));
                    reply.append(QnStatisticsDataItem(
                        interfaceName + QChar(0x21e9),
                        extractXmlBody(interfaceBlock, "in").toDouble(),
                        NETWORK_IN
                    ));
                    reply.append(QnStatisticsDataItem(
                        interfaceName + QChar(0x21e7),
                        extractXmlBody(interfaceBlock, "out").toDouble(),
                        NETWORK_OUT
                    ));
                } while (networkBlock.length() > 0);
            }
        }
        
        emitFinished(status, reply, handle); 
        break;
    }
    case PtzSpaceMapperObject: {
        int status = response.status;

        QnPtzSpaceMapper reply;
        if(status == 0) {
            QVariantMap map;
            if(!QJson::deserialize(response.data, &map) || !QJson::deserialize(map, "reply", &reply))
                status = 1;
        } else {
            qnWarning("Could not get ptz space mapper for camera: %1.", response.errorString);
        }

        emitFinished(status, reply, handle);
        break;
    }
    case PtzPositionObject: {
        const QByteArray& data = response.data;
        QVector3D reply;

        if(response.status == 0) {
            reply.setX(extractXmlBody(data, "xPos").toDouble());
            reply.setY(extractXmlBody(data, "yPos").toDouble());
            reply.setZ(extractXmlBody(data, "zoomPos").toDouble());
        } else {
            qnWarning("Could not get ptz position from camera: %1.", response.errorString);
        }

        emitFinished(response.status, reply, handle);
        break;
    }
    case PtzSetPositionObject:
    case PtzStopObject:
    case PtzMoveObject: {
        emitFinished(response.status, handle);
        break;
    }
    default:
        break; // TODO: #Elric warning?
    }

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

} // namespace detail

// ---------------------------------- QnMediaServerConnection ---------------------

QnMediaServerConnection::QnMediaServerConnection(const QUrl &mediaServerApiUrl, QObject *parent):
    QObject(parent),
    m_nameMapper(new QnEnumNameMapper(createEnumNameMapper<RequestObject>())),
    m_url(mediaServerApiUrl),
    m_proxyPort(0)
{
}

QnMediaServerConnection::~QnMediaServerConnection() {
    return;
}

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

void QnMediaServerConnection::setProxyAddr(const QUrl& apiUrl, const QString& addr, int port)
{
    m_proxyAddr = addr;
    m_proxyPort = port;

    if (port)
        QnNetworkProxyFactory::instance()->addToProxyList(apiUrl, addr, port);
    else
        QnNetworkProxyFactory::instance()->removeFromProxyList(apiUrl);
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

int QnMediaServerConnection::asyncPtzMove(const QnNetworkResourcePtr &camera, const QVector3D &speed, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot) {
    QnMediaServerReplyProcessor *processor = new QnMediaServerReplyProcessor(PtzMoveObject);
    connect(processor, SIGNAL(finished(int, int)), target, slot, Qt::QueuedConnection);

    QnRequestParamList params;
    params << QnRequestParam("res_id",  camera->getPhysicalId());
    params << QnRequestParam("xSpeed",  QString::number(speed.x()));
    params << QnRequestParam("ySpeed",  QString::number(speed.y()));
    params << QnRequestParam("zoomSpeed", QString::number(speed.z()));
    params << QnRequestParam("seqId",   sequenceId.toString());
    params << QnRequestParam("seqNum",  QString::number(sequenceNumber));

    return sendAsyncRequest(processor, params);
}

int QnMediaServerConnection::asyncPtzStop(const QnNetworkResourcePtr &camera, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot) {
    QnMediaServerReplyProcessor *processor = new QnMediaServerReplyProcessor(PtzStopObject);
    connect(processor, SIGNAL(finished(int, int)), target, slot, Qt::QueuedConnection);

    QnRequestParamList params;
    params << QnRequestParam("res_id",  camera->getPhysicalId());
    params << QnRequestParam("seqId",   sequenceId.toString());
    params << QnRequestParam("seqNum",  QString::number(sequenceNumber));

    return sendAsyncRequest(processor, params);
}

int QnMediaServerConnection::asyncPtzMoveTo(const QnNetworkResourcePtr &camera, const QVector3D &pos, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot) {
    QnMediaServerReplyProcessor *processor = new QnMediaServerReplyProcessor(PtzSetPositionObject);
    connect(processor, SIGNAL(finished(int, int)), target, slot, Qt::QueuedConnection);

    QnRequestParamList params;
    params << QnRequestParam("res_id",  camera->getPhysicalId());
    params << QnRequestParam("xPos",    QString::number(pos.x()));
    params << QnRequestParam("yPos",    QString::number(pos.y()));
    params << QnRequestParam("zoomPos", QString::number(pos.z()));
    params << QnRequestParam("seqId",   sequenceId.toString());
    params << QnRequestParam("seqNum",  QString::number(sequenceNumber));

    return sendAsyncRequest(processor, params);
}

int QnMediaServerConnection::asyncPtzGetPos(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    QnMediaServerReplyProcessor *processor = new QnMediaServerReplyProcessor(PtzPositionObject);
    connect(processor, SIGNAL(finished(int, const QVector3D &, int)), target, slot, Qt::QueuedConnection);

    QnRequestParamList params;
    params << QnRequestParam("res_id",  camera->getPhysicalId());

    return sendAsyncRequest(processor, params);
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

int QnMediaServerConnection::asyncRecordedTimePeriods(const QnRequestParamList &params, QObject *target, const char *slot)
{
    QnMediaServerReplyProcessor *processor = new QnMediaServerReplyProcessor(TimePeriodsObject);
    connect(processor, SIGNAL(finished(int, const QnTimePeriodList &, int)), target, slot, Qt::QueuedConnection);

    return sendAsyncRequest(processor, params);
}

int QnMediaServerConnection::asyncRecordedTimePeriods(const QnNetworkResourceList &list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, const QList<QRegion> &motionRegions, QObject *target, const char *slot) 
{
    return asyncRecordedTimePeriods(createParamList(list, startTimeMs, endTimeMs, detail, motionRegions), target, slot);
}

int QnMediaServerConnection::asyncGetStorageSpace(QObject *target, const char *slot) {
    QnMediaServerReplyProcessor *processor = new QnMediaServerReplyProcessor(StorageSpaceObject);
    connect(processor, SIGNAL(finished(int, const QnStorageSpaceReply &, int)), target, slot, Qt::QueuedConnection);

    return sendAsyncRequest(processor);
}

int QnMediaServerConnection::asyncGetStorageStatus(const QString &storageUrl, QObject *target, const char *slot) {
    QnMediaServerReplyProcessor *processor = new QnMediaServerReplyProcessor(StorageStatusObject);
    connect(processor, SIGNAL(finished(int, const QnStorageStatusReply &, int)), target, slot, Qt::QueuedConnection);

    QnRequestParamList params;
    params << QnRequestParam("path", storageUrl);

    return sendAsyncRequest(processor, params);
}

int QnMediaServerConnection::asyncGetStatistics(QObject *target, const char *slot){
    QnMediaServerReplyProcessor *processor = new QnMediaServerReplyProcessor(StatisticsObject);
    connect(processor, SIGNAL(finished(int, const QnStatisticsDataList &, int)), target, slot, Qt::QueuedConnection);

    return sendAsyncRequest(processor);
}

int QnMediaServerConnection::asyncPtzGetSpaceMapper(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    QnMediaServerReplyProcessor *processor = new QnMediaServerReplyProcessor(PtzSpaceMapperObject);
    connect(processor, SIGNAL(finished(int, const QnPtzSpaceMapper &, int)), target, slot, Qt::QueuedConnection);

    QnRequestParamList params;
    params << QnRequestParam("res_id", camera->getPhysicalId());

    return sendAsyncRequest(processor, params);
}

int QnMediaServerConnection::sendAsyncRequest(QnMediaServerReplyProcessor *processor, const QnRequestParamList &params, const QnRequestHeaderList &headers) {
    return QnSessionManager::instance()->sendAsyncGetRequest(
        m_url, 
        m_nameMapper->name(processor->object()), 
        headers, 
        params, 
        processor, 
        SLOT(processReply(QnHTTPRawResponse, int))
    );
}

bool QnMediaServerConnection::connect(QnMediaServerReplyProcessor *sender, const char *signal, QObject *receiver, const char *method, Qt::ConnectionType connectionType) {
    if(strstr(method, "QVariant")) {
        return base_type::connect(sender, SIGNAL(finished(int, const QVariant &, int)), receiver, method, connectionType);
    } else {
        return base_type::connect(sender, signal, receiver, method, connectionType);
    }
}


