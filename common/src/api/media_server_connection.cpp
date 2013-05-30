#include "media_server_connection.h"

#include <cstring> /* For std::strstr. */

#include <boost/preprocessor/stringize.hpp>

#include <QNetworkProxy>
#include <QNetworkReply>
#include <QSharedPointer>

#include "utils/common/util.h"
#include "utils/common/warnings.h"
#include "utils/common/request_param.h"
#include "utils/math/space_mapper.h"
#include "utils/common/json.h"
#include "utils/common/enum_name_mapper.h"

#include "session_manager.h"

#include <api/serializer/serializer.h>
#include "serializer/pb_serializer.h"
#include "event_log/events_serializer.h"

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
        ((TimeObject,               "gettime"))
        ((CameraSearchObject,       "manualCamera/search"))
        ((CameraAddObject,          "manualCamera/add"))
        ((eventLogObject,           "events"))
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
void QnMediaServerReplyProcessor::processReply(const QnHTTPRawResponse &response, int handle) {
    switch(object()) {
    case StorageStatusObject:
        processJsonReply<QnStorageStatusReply>(this, response, handle);
        break;
    case StorageSpaceObject:
        processJsonReply<QnStorageSpaceReply>(this, response, handle);
        break;
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

        emitFinished(this, status, reply, handle);
        break;
    }
    case StatisticsObject: {
        const QByteArray &data = response.data;
        int status = response.status;
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
                        extractXmlBody(interfaceBlock, "in").toDouble() * 8, //converting from bytes to bits
                        NETWORK_IN
                    ));
                    reply.statistics.append(QnStatisticsDataItem(
                        interfaceName + QChar(0x21e7),
                        extractXmlBody(interfaceBlock, "out").toDouble() * 8, //converting from bytes to bits
                        NETWORK_OUT
                    ));
                } while (networkBlock.length() > 0);
            }

            QByteArray paramsBlock = extractXmlBody(data, "params");
            reply.updatePeriod = extractXmlBody(paramsBlock, "updatePeriod").toInt();
        }
        
        emitFinished(this, status, reply, handle);
        break;
    }
    case PtzSpaceMapperObject:
        processJsonReply<QnPtzSpaceMapper>(this, response, handle);
        break;
    case PtzPositionObject: {
        const QByteArray& data = response.data;
        QVector3D reply;

        if(response.status == 0) {
            reply.setX(extractXmlBody(data, "xPos").toDouble());
            reply.setY(extractXmlBody(data, "yPos").toDouble());
            reply.setZ(extractXmlBody(data, "zoomPos").toDouble());
        } else {
//            qnWarning("Could not get ptz position from camera: %1.", response.errorString);
        }

        emitFinished(this, response.status, reply, handle);
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

        emitFinished(this, response.status, reply, handle);
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

        emitFinished(this, response.status, reply, handle);
        break;
    }
    case TimeObject:
        processJsonReply<QnTimeReply>(this, response, handle);
        break;
    case PtzSetPositionObject:
    case PtzStopObject:
    case PtzMoveObject:
    case CameraAddObject: 
        emitFinished(this, response.status, handle);
        break;
    case CameraSearchObject: {
        QnCamerasFoundInfoList reply;

        if (response.status == 0) {
            QByteArray root = extractXmlBody(response.data, "reply");
            QByteArray resource;
            int from = 0;
            do {
                resource = extractXmlBody(root, "resource", &from);
                if (resource.length() == 0)
                    break;
                QString url = QLatin1String(extractXmlBody(resource, "url"));
                QString name = QLatin1String(extractXmlBody(resource, "name"));
                QString manufacture = QLatin1String(extractXmlBody(resource, "manufacturer"));
                reply.append(QnCamerasFoundInfo(url, name, manufacture));
            } while (resource.length() > 0);
        } else {
            qnWarning("Camera search failed: %1.", extractXmlBody(response.data, "root"));
        }

        emitFinished(this, response.status, reply, handle);
        break;
    }
    case eventLogObject: {
        QnApiPbSerializer serializer;
        QnBusinessActionDataListPtr events(new QnBusinessActionDataList);
        if (response.status == 0)
            QnEventSerializer::deserialize(events, response.data);
        emitFinished(this, response.status, events, handle);
		break;
    }
    default:
        assert(false); /* We should never get here. */
        break;
    }

    deleteLater();
}


// -------------------------------------------------------------------------- //
// QnMediaServerConnection
// -------------------------------------------------------------------------- //
QnMediaServerConnection::QnMediaServerConnection(const QUrl &mediaServerApiUrl, QObject *parent):
    base_type(parent),
    m_proxyPort(0)
{
    setUrl(mediaServerApiUrl);
    setNameMapper(new QnEnumNameMapper(createEnumNameMapper<RequestObject>()));
}

QnMediaServerConnection::~QnMediaServerConnection() {
    return;
}

QnAbstractReplyProcessor *QnMediaServerConnection::newReplyProcessor(int object) {
    return new QnMediaServerReplyProcessor(object);
}

void QnMediaServerConnection::setProxyAddr(const QUrl &apiUrl, const QString &addr, int port) {
    m_proxyAddr = addr;
    m_proxyPort = port;

    if (port) {
        QnNetworkProxyFactory::instance()->addToProxyList(apiUrl, addr, port);
    } else {
        QnNetworkProxyFactory::instance()->removeFromProxyList(apiUrl);
    }
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
    return sendAsyncGetRequest(TimePeriodsObject, createTimePeriodsRequest(list, startTimeMs, endTimeMs, detail, motionRegions), QN_REPLY_TYPE(QnTimePeriodList), target, slot);
}

QnRequestParamList QnMediaServerConnection::createGetParamsRequest(const QnNetworkResourcePtr &camera, const QStringList &params) {
    QnRequestParamList result;
    result << QnRequestParam("res_id", camera->getPhysicalId());
    foreach(QString param, params)
        result << QnRequestParam(param, QString());
    return result;
}

int QnMediaServerConnection::getParamsAsync(const QnNetworkResourcePtr &camera, const QStringList &keys, QObject *target, const char *slot) {
    return sendAsyncGetRequest(GetParamsObject, createGetParamsRequest(camera, keys), QN_REPLY_TYPE(QnStringVariantPairList), target, slot);
}

int QnMediaServerConnection::getParamsSync(const QnNetworkResourcePtr &camera, const QStringList &keys, QnStringVariantPairList *reply) {
    return sendSyncGetRequest(GetParamsObject, createGetParamsRequest(camera, keys), reply);
}

QnRequestParamList QnMediaServerConnection::createSetParamsRequest(const QnNetworkResourcePtr &camera, const QnStringVariantPairList &params) {
    QnRequestParamList result;
    result << QnRequestParam("res_id", camera->getPhysicalId());
    for(QnStringVariantPairList::const_iterator i = params.begin(); i != params.end(); ++i) 
        result << QnRequestParam(i->first, i->second.toString());
    return result;
}

int QnMediaServerConnection::setParamsAsync(const QnNetworkResourcePtr &camera, const QnStringVariantPairList &params, QObject *target, const char *slot) {
    return sendAsyncGetRequest(SetParamsObject, createSetParamsRequest(camera, params), QN_REPLY_TYPE(QnStringBoolPairList), target, slot);
}

int QnMediaServerConnection::setParamsSync(const QnNetworkResourcePtr &camera, const QnStringVariantPairList &params, QnStringBoolPairList *reply) {
    return sendSyncGetRequest(SetParamsObject, createSetParamsRequest(camera, params), reply);
}

int QnMediaServerConnection::searchCameraAsync(const QString &startAddr, const QString &endAddr, const QString &username, const QString &password, int port, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("start_ip", startAddr);
    if (!endAddr.isEmpty())
        params << QnRequestParam("end_ip", endAddr);
    params << QnRequestParam("user", username);
    params << QnRequestParam("password", password);
    params << QnRequestParam("port" ,QString::number(port));

    return sendAsyncGetRequest(CameraSearchObject, params, QN_REPLY_TYPE(QnCamerasFoundInfoList), target, slot);
}

int QnMediaServerConnection::addCameraAsync(const QStringList &urls, const QStringList &manufacturers, const QString &username, const QString &password, QObject *target, const char *slot) {
    QnRequestParamList params;
    for (int i = 0; i < qMin(urls.count(), manufacturers.count()); i++){
        params << QnRequestParam("url", urls[i]);
        params << QnRequestParam("manufacturer", manufacturers[i]);
    }
    params << QnRequestParam("user", username);
    params << QnRequestParam("password", password);

    return sendAsyncGetRequest(CameraAddObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzMoveAsync(const QnNetworkResourcePtr &camera, const QVector3D &speed, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("res_id",  camera->getPhysicalId());
    params << QnRequestParam("xSpeed",  QString::number(speed.x()));
    params << QnRequestParam("ySpeed",  QString::number(speed.y()));
    params << QnRequestParam("zoomSpeed", QString::number(speed.z()));
    params << QnRequestParam("seqId",   sequenceId.toString());
    params << QnRequestParam("seqNum",  QString::number(sequenceNumber));

    return sendAsyncGetRequest(PtzMoveObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzStopAsync(const QnNetworkResourcePtr &camera, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("res_id",  camera->getPhysicalId());
    params << QnRequestParam("seqId",   sequenceId.toString());
    params << QnRequestParam("seqNum",  QString::number(sequenceNumber));

    return sendAsyncGetRequest(PtzStopObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzMoveToAsync(const QnNetworkResourcePtr &camera, const QVector3D &pos, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("res_id",  camera->getPhysicalId());
    params << QnRequestParam("xPos",    QString::number(pos.x()));
    params << QnRequestParam("yPos",    QString::number(pos.y()));
    params << QnRequestParam("zoomPos", QString::number(pos.z()));
    params << QnRequestParam("seqId",   sequenceId.toString());
    params << QnRequestParam("seqNum",  QString::number(sequenceNumber));

    return sendAsyncGetRequest(PtzSetPositionObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzGetPosAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("res_id",  camera->getPhysicalId());

    return sendAsyncGetRequest(PtzPositionObject, params, QN_REPLY_TYPE(QVector3D), target, slot);
}

int QnMediaServerConnection::getTimeAsync(QObject *target, const char *slot) {
    return sendAsyncGetRequest(TimeObject, QnRequestParamList(), QN_REPLY_TYPE(QnTimeReply), target, slot);
}

int QnMediaServerConnection::getStorageSpaceAsync(QObject *target, const char *slot) {
    return sendAsyncGetRequest(StorageSpaceObject, QnRequestParamList(), QN_REPLY_TYPE(QnStorageSpaceReply), target, slot);
}

int QnMediaServerConnection::getStorageStatusAsync(const QString &storageUrl, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("path", storageUrl);

    return sendAsyncGetRequest(StorageStatusObject, params, QN_REPLY_TYPE(QnStorageStatusReply), target, slot);
}

int QnMediaServerConnection::getStatisticsAsync(QObject *target, const char *slot){
    return sendAsyncGetRequest(StatisticsObject, QnRequestParamList(), QN_REPLY_TYPE(QnStatisticsReply), target, slot);
}

int QnMediaServerConnection::ptzGetSpaceMapperAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("res_id", camera->getPhysicalId());

    return sendAsyncGetRequest(PtzSpaceMapperObject, params, QN_REPLY_TYPE(QnPtzSpaceMapper), target, slot);
}

int QnMediaServerConnection::asyncEventLog(
                  qint64 dateFrom, qint64 dateTo, 
                  QnResourceList camList,
                  BusinessEventType::Value eventType, 
                  BusinessActionType::Value actionType,
                  QnId businessRuleId, 
                  QObject *target, const char *slot)
{
    QnRequestParamList params;
    params << QnRequestParam( "from", dateFrom);
    if (dateTo != DATETIME_NOW)
        params << QnRequestParam( "to", dateTo);
    foreach(QnResourcePtr res, camList) {
        QnNetworkResourcePtr camera = res.dynamicCast<QnNetworkResource>();
        if (camera)
            params << QnRequestParam( "res_id", camera->getPhysicalId() );
    }
    if (businessRuleId.isValid())
        params << QnRequestParam( "brule_id", businessRuleId.toInt() );
    if (eventType != BusinessEventType::NotDefined)
        params << QnRequestParam( "event", (int) eventType);
    if (actionType != BusinessActionType::NotDefined)
        params << QnRequestParam( "action", (int) actionType);

    return sendAsyncGetRequest(eventLogObject, params, QN_REPLY_TYPE(QnBusinessActionDataListPtr), target, slot);
}

