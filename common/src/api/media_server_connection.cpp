#include "media_server_connection.h"

#include <cstring> /* For std::strstr. */

#include <boost/preprocessor/stringize.hpp>

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
        ((GetParamsObject,          "getCameraParam"))
        ((SetParamsObject,          "setCameraParam"))
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


    template<class T>
    const char *check_reply_type() { return NULL; }

} // anonymous namespace


/**
 * Macro that stringizes the given type name and checks at compile time that
 * the given type is actually defined.
 * 
 * \param TYPE                          Type to stringize.
 */
#define QN_REPLY_TYPE(TYPE) (check_reply_type<TYPE>(), BOOST_PP_STRINGIZE(TYPE))


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
    m_object(object),
    m_finished(false)
{}

QnMediaServerReplyProcessor::~QnMediaServerReplyProcessor() {
    return;
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
        int updatePeriod = -1;
        QnStatisticsReply reply;

        if(status == 0) {
            QByteArray cpuBlock = extractXmlBody(data, "cpuinfo");
            reply.statistics.append(QnStatisticsDataItem(
                QLatin1String("CPU"), 
                extractXmlBody(cpuBlock, "load").toDouble(), 
                CPU
            ));

            QByteArray memoryBlock = extractXmlBody(data, "memory");
            reply.statistics.append(QnStatisticsDataItem(
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
                    reply.statistics.append(QnStatisticsDataItem(
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
                    reply.statistics.append(QnStatisticsDataItem(
                        interfaceName + QChar(0x21e9),
                        extractXmlBody(interfaceBlock, "in").toDouble(),
                        NETWORK_IN
                    ));
                    reply.statistics.append(QnStatisticsDataItem(
                        interfaceName + QChar(0x21e7),
                        extractXmlBody(interfaceBlock, "out").toDouble(),
                        NETWORK_OUT
                    ));
                } while (networkBlock.length() > 0);
            }

            QByteArray paramsBlock = extractXmlBody(data, "params");
            reply.updatePeriod = extractXmlBody(paramsBlock, "updatePeriod").toInt();
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
    case GetParamsObject: {
        QnStringVariantPairList reply;

        foreach(const QByteArray &line, response.data.split('\n')) {
            int sepPos = line.indexOf('=');
            if(sepPos == -1) { 
                reply.push_back(qMakePair(QString::fromUtf8(line.constData(), line.size()), QVariant())); /* No value. */
            } else {
                QByteArray key = line.mid(0, sepPos);
                QByteArray value = line.mid(sepPos + 1);
                reply.push_back(qMakePair(
                    QString::fromUtf8(key.constData(), key.size()), 
                    QVariant(QString::fromUtf8(value.constData(), value.size()))
                ));
            }
        }

        emitFinished(response.status, reply, handle);
        break;
    }
    case SetParamsObject: {
        QnStringBoolPairList reply;

        foreach(const QByteArray &line, response.data.split('\n')) {
            int sepPos = line.indexOf(':');
            if(sepPos == -1) 
                continue; /* Invalid format. */

            QByteArray key = line.mid(sepPos+1);
            reply.push_back(qMakePair(
                QString::fromUtf8(key.data(), key.size()),
                line.mid(0, sepPos) == "ok"
            ));
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


// -------------------------------------------------------------------------- //
// QnMediaServerConnection
// -------------------------------------------------------------------------- //
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

QnRequestParamList QnMediaServerConnection::createTimePeriodsRequest(const QnNetworkResourceList &list, qint64 startTimeUSec, qint64 endTimeUSec, qint64 detail, const QList<QRegion>& motionRegions) {
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

int QnMediaServerConnection::getTimePeriodsAsync(const QnNetworkResourceList &list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, const QList<QRegion> &motionRegions, QObject *target, const char *slot) {
    return sendAsyncRequest(TimePeriodsObject, createTimePeriodsRequest(list, startTimeMs, endTimeMs, detail, motionRegions), QN_REPLY_TYPE(QnTimePeriodList), target, slot);
}

QnRequestParamList QnMediaServerConnection::createGetParamsRequest(const QnNetworkResourcePtr &camera, const QStringList &params) {
    QnRequestParamList result;
    result << QnRequestParam("res_id", camera->getPhysicalId());
    foreach(QString param, params)
        result << QnRequestParam(param, QString());
    return result;
}

int QnMediaServerConnection::getParamsAsync(const QnNetworkResourcePtr &camera, const QStringList &keys, QObject *target, const char *slot) {
    return sendAsyncRequest(GetParamsObject, createGetParamsRequest(camera, keys), QN_REPLY_TYPE(QnStringVariantPairList), target, slot);
}

int QnMediaServerConnection::getParamsSync(const QnNetworkResourcePtr &camera, const QStringList &keys, QnStringVariantPairList *reply) {
    return sendSyncRequest(GetParamsObject, createGetParamsRequest(camera, keys), reply);
}

QnRequestParamList QnMediaServerConnection::createSetParamsRequest(const QnNetworkResourcePtr &camera, const QnStringVariantPairList &params) {
    QnRequestParamList result;
    result << QnRequestParam("res_id", camera->getPhysicalId());
    for(QnStringVariantPairList::const_iterator i = params.begin(); i != params.end(); ++i) 
        result << QnRequestParam(i->first, i->second.toString());
    return result;
}

int QnMediaServerConnection::setParamsAsync(const QnNetworkResourcePtr &camera, const QnStringVariantPairList &params, QObject *target, const char *slot) {
    return sendAsyncRequest(SetParamsObject, createSetParamsRequest(camera, params), QN_REPLY_TYPE(QnStringBoolPairList), target, slot);
}

int QnMediaServerConnection::setParamsAsync(const QnNetworkResourcePtr &camera, const QnStringVariantPairList &params, QnStringBoolPairList *reply) {
    return sendSyncRequest(SetParamsObject, createSetParamsRequest(camera, params), reply);
}

int QnMediaServerConnection::searchCameraAsync(const QString &startAddr, const QString &endAddr, const QString& username, const QString &password, const int port,
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

int QnMediaServerConnection::addCameraAsync(const QStringList &urls, const QStringList &manufacturers, const QString &username, const QString &password,
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

void QnMediaServerConnection::setProxyAddr(const QUrl &apiUrl, const QString &addr, int port)
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

int QnMediaServerConnection::ptzMoveAsync(const QnNetworkResourcePtr &camera, const QVector3D &speed, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("res_id",  camera->getPhysicalId());
    params << QnRequestParam("xSpeed",  QString::number(speed.x()));
    params << QnRequestParam("ySpeed",  QString::number(speed.y()));
    params << QnRequestParam("zoomSpeed", QString::number(speed.z()));
    params << QnRequestParam("seqId",   sequenceId.toString());
    params << QnRequestParam("seqNum",  QString::number(sequenceNumber));

    return sendAsyncRequest(PtzMoveObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzStopAsync(const QnNetworkResourcePtr &camera, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("res_id",  camera->getPhysicalId());
    params << QnRequestParam("seqId",   sequenceId.toString());
    params << QnRequestParam("seqNum",  QString::number(sequenceNumber));

    return sendAsyncRequest(PtzStopObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzMoveToAsync(const QnNetworkResourcePtr &camera, const QVector3D &pos, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("res_id",  camera->getPhysicalId());
    params << QnRequestParam("xPos",    QString::number(pos.x()));
    params << QnRequestParam("yPos",    QString::number(pos.y()));
    params << QnRequestParam("zoomPos", QString::number(pos.z()));
    params << QnRequestParam("seqId",   sequenceId.toString());
    params << QnRequestParam("seqNum",  QString::number(sequenceNumber));

    return sendAsyncRequest(PtzSetPositionObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzGetPosAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("res_id",  camera->getPhysicalId());

    return sendAsyncRequest(PtzPositionObject, params, QN_REPLY_TYPE(QVector3D), target, slot);
}

int QnMediaServerConnection::getTimeAsync(QObject *target, const char *slot) {
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

int QnMediaServerConnection::getStorageSpaceAsync(QObject *target, const char *slot) {
    return sendAsyncRequest(StorageSpaceObject, QnRequestParamList(), QN_REPLY_TYPE(QnStorageSpaceReply), target, slot);
}

int QnMediaServerConnection::getStorageStatusAsync(const QString &storageUrl, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("path", storageUrl);

    return sendAsyncRequest(StorageStatusObject, params, QN_REPLY_TYPE(QnStorageStatusReply), target, slot);
}

int QnMediaServerConnection::getStatisticsAsync(QObject *target, const char *slot){
    return sendAsyncRequest(StatisticsObject, QnRequestParamList(), QN_REPLY_TYPE(QnStatisticsReply), target, slot);
}

int QnMediaServerConnection::ptzGetSpaceMapperAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("res_id", camera->getPhysicalId());

    return sendAsyncRequest(PtzSpaceMapperObject, params, QN_REPLY_TYPE(QnPtzSpaceMapper), target, slot);
}

int QnMediaServerConnection::sendAsyncRequest(int object, const QnRequestParamList &params, const char *replyTypeName, QObject *target, const char *slot) {
    QnMediaServerReplyProcessor *processor = new QnMediaServerReplyProcessor(object);

    QByteArray signal;
    if(replyTypeName == NULL) {
        signal = SIGNAL(finished(int, int));
    } else {
        signal = lit("%1finished(int, const %2 &, int)").arg(QSIGNAL_CODE).arg(QLatin1String(replyTypeName)).toLatin1();
    }
    connect(processor, signal.constData(), target, slot, Qt::QueuedConnection);

    return QnSessionManager::instance()->sendAsyncGetRequest(
        m_url, 
        m_nameMapper->name(processor->object()), 
        QnRequestHeaderList(), 
        params, 
        processor, 
        SLOT(processReply(QnHTTPRawResponse, int))
    );
}

int QnMediaServerConnection::sendSyncRequest(int object, const QnRequestParamList &params, QVariant *reply) {
    assert(reply);

    QnHTTPRawResponse response;
    QnSessionManager::instance()->sendGetRequest(
        m_url,
        m_nameMapper->name(object),
        QnRequestHeaderList(),
        params,
        response
    );

    QnMediaServerReplyProcessor processor(object);
    processor.processReply(response, -1);
    *reply = processor.reply();

    return processor.status();
}

bool QnMediaServerConnection::connect(QnMediaServerReplyProcessor *sender, const char *signal, QObject *receiver, const char *method, Qt::ConnectionType connectionType) {
    if(strstr(method, "QVariant")) {
        return base_type::connect(sender, SIGNAL(finished(int, const QVariant &, int)), receiver, method, connectionType);
    } else {
        return base_type::connect(sender, signal, receiver, method, connectionType);
    }
}


