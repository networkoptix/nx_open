#include "media_server_connection.h"

#include <cstring> /* For std::strstr. */

#include <QtCore/QSharedPointer>
#include <QtCore/QUuid>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtGui/QImage>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QNetworkReply>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_bookmark.h>

#include <utils/common/util.h>
#include <utils/common/warnings.h>
#include <utils/common/request_param.h>
#include <utils/common/model_functions.h>

#include <api/serializer/serializer.h>
#include <event_log/events_serializer.h>

#include <recording/time_period_list.h>

#include "network_proxy_factory.h"
#include "session_manager.h"

namespace {
    QN_DEFINE_LEXICAL_ENUM(RequestObject,
        (StorageStatusObject,      "storageStatus")
        (StorageSpaceObject,       "storageSpace")
        (TimePeriodsObject,        "RecordedTimePeriods")
        (StatisticsObject,         "statistics")
        (PtzContinuousMoveObject,  "ptz")
        (PtzAbsoluteMoveObject,    "ptz")
        (PtzViewportMoveObject,    "ptz")
        (PtzGetPositionObject,     "ptz")
        (PtzCreatePresetObject,    "ptz")
        (PtzUpdatePresetObject,    "ptz")
        (PtzRemovePresetObject,    "ptz")
        (PtzActivatePresetObject,  "ptz")
        (PtzGetPresetsObject,      "ptz")
        (PtzCreateTourObject,      "ptz")
        (PtzRemoveTourObject,      "ptz")
        (PtzActivateTourObject,    "ptz")
        (PtzGetToursObject,        "ptz")
        (PtzGetHomeObjectObject,   "ptz")
        (PtzGetActiveObjectObject, "ptz")
        (PtzUpdateHomeObjectObject, "ptz")
        (PtzGetDataObject,         "ptz")
        (GetParamsObject,          "getCameraParam")
        (SetParamsObject,          "setCameraParam")
        (TimeObject,               "gettime")
        (CameraSearchStartObject,  "manualCamera/search")
        (CameraSearchStatusObject, "manualCamera/status")
        (CameraSearchStopObject,   "manualCamera/stop")
        (CameraAddObject,          "manualCamera/add")
        (EventLogObject,           "events")
        (ImageObject,              "image")
        (CameraDiagnosticsObject,  "doCameraDiagnosticsStep")
        (RebuildArchiveObject,     "rebuildArchive")
        (BookmarkAddObject,        "cameraBookmarks/add")
        (BookmarkUpdateObject,     "cameraBookmarks/update")
        (BookmarkDeleteObject,     "cameraBookmarks/delete")
        (BookmarksGetObject,       "cameraBookmarks/get")
        (ChangeSystemNameObject,   "changeSystemName")
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
/*
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

        QString urlPath = query.url().path();
        if (urlPath.startsWith(QLatin1String("/")))
            urlPath.remove(0, 1);

        if (urlPath.endsWith(QLatin1String("/")))
            urlPath.chop(1);

        if (urlPath.isEmpty() || urlPath == QLatin1String("api/ping")) {
            rez << QNetworkProxy(QNetworkProxy::NoProxy);
            return rez;
        }
        QString host = query.peerHostName();
        QUrl url = query.url();
        url.setPath(QString());
        url.setUserInfo(QString());
        url.setQuery(QUrlQuery());

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

QPointer<QnNetworkProxyFactory> createGlobalProxyFactory() {
    QnNetworkProxyFactory *result(new QnNetworkProxyFactory());

    // Qt will take ownership of the supplied instance. 
    QNetworkProxyFactory::setApplicationProxyFactory(result); // TODO: #Elric we have a race if this code is run several times from different threads.

    return result;
}

Q_GLOBAL_STATIC_WITH_ARGS(QPointer<QnNetworkProxyFactory>, qn_globalProxyFactory, (createGlobalProxyFactory()));

QnNetworkProxyFactory *QnNetworkProxyFactory::instance()
{
    QPointer<QnNetworkProxyFactory> *result = qn_globalProxyFactory();
    if(*result) {
        return result->data();
    } else {
        return qn_reserveProxyFactory();
    }
}


*/

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
                reply.decode((const quint8*) response.data.constData() + 3, response.data.size() - 3, false);
            } else if (response.data.startsWith("BII")) {
                reply.decode((const quint8*) response.data.constData() + 3, response.data.size() - 3, true);
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

            QByteArray miscBlock = extractXmlBody(data, "misc");
            reply.uptimeMs = extractXmlBody(miscBlock, "uptimeMs").toLongLong();

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
                    int interfaceType = extractXmlBody(interfaceBlock, "type").toInt();
                    qint64 bytesIn = extractXmlBody(interfaceBlock, "in").toLongLong();
                    qint64 bytesOut = extractXmlBody(interfaceBlock, "out").toLongLong();
                    qint64 bytesMax = extractXmlBody(interfaceBlock, "max").toLongLong();

                    if(bytesMax != 0)
                        reply.statistics.push_back(QnStatisticsDataItem(interfaceName, static_cast<qreal>(qMax(bytesIn, bytesOut)) / bytesMax, NETWORK, interfaceType));
                } while (networkBlock.length() > 0);
            }

            QByteArray paramsBlock = extractXmlBody(data, "params");
            reply.updatePeriod = extractXmlBody(paramsBlock, "updatePeriod").toInt();
        }

        emitFinished(this, status, reply, handle);
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
    case CameraAddObject:
        emitFinished(this, response.status, handle);
        break;
    case PtzContinuousMoveObject:
    case PtzAbsoluteMoveObject:
    case PtzViewportMoveObject:
    case PtzCreatePresetObject:
    case PtzUpdatePresetObject:
    case PtzRemovePresetObject:
    case PtzActivatePresetObject:
    case PtzCreateTourObject:
    case PtzRemoveTourObject:
    case PtzActivateTourObject:
    case PtzUpdateHomeObjectObject:
        emitFinished(this, response.status, handle);
        break;
    case PtzGetPositionObject:
        processJsonReply<QVector3D>(this, response, handle);
        break;
    case PtzGetPresetsObject:
        processJsonReply<QnPtzPresetList>(this, response, handle);
        break;
    case PtzGetToursObject:
        processJsonReply<QnPtzTourList>(this, response, handle);
        break;
    case PtzGetActiveObjectObject:
    case PtzGetHomeObjectObject:
        processJsonReply<QnPtzObject>(this, response, handle);
        break;
    case PtzGetDataObject:
        processJsonReply<QnPtzData>(this, response, handle);
        break;
    case CameraSearchStartObject:
    case CameraSearchStatusObject:
    case CameraSearchStopObject:
        processJsonReply<QnManualCameraSearchReply>(this, response, handle);
        break;
    case EventLogObject: {
        QnBusinessActionDataListPtr events(new QnBusinessActionDataList);
        if (response.status == 0)
            QnEventSerializer::deserialize(events, response.data);
        emitFinished(this, response.status, events, handle);
        break;
    }
    case ImageObject: {
        QImage image;
        if (response.status == 0)
            image.loadFromData(response.data);
        emitFinished(this, response.status, image, handle);
        break;
    }
    case CameraDiagnosticsObject:
        processJsonReply<QnCameraDiagnosticsReply>(this, response, handle);
        break;
    case RebuildArchiveObject: {
        QnRebuildArchiveReply info;
        if (response.status == 0)
            info.deserialize(response.data);
        emitFinished(this, response.status, info, handle);
        break;
    }
    case BookmarkAddObject: 
    case BookmarkUpdateObject: 
    case BookmarkDeleteObject:
        processJsonReply<QnCameraBookmark>(this, response, handle);
        break;
    case BookmarksGetObject:
        processJsonReply<QnCameraBookmarkList>(this, response, handle);
        break;
    case ChangeSystemNameObject:
        emitFinished(this, response.status, handle);
        break;
    default:
        assert(false); /* We should never get here. */
        break;
    }

    deleteLater();
}


// -------------------------------------------------------------------------- //
// QnMediaServerConnection
// -------------------------------------------------------------------------- //
QnMediaServerConnection::QnMediaServerConnection(QnMediaServerResource* mserver, const QString &videoWallKey, QObject *parent):
    base_type(parent),
    m_proxyPort(0)
{
    setUrl(mserver->getApiUrl());
    setSerializer(QnLexical::newEnumSerializer<RequestObject, int>());

    QnRequestHeaderList extraHeaders;
    extraHeaders.insert(lit("x-server-guid"), mserver->getId().toString());
    if (!videoWallKey.isEmpty())
        extraHeaders.insert(lit("X-NetworkOptix-VideoWall"), videoWallKey);
    setExtraHeaders(extraHeaders);
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

int QnMediaServerConnection::getThumbnailAsync(const QnNetworkResourcePtr &camera, qint64 timeUsec, const
                                                QSize& size, const QString& imageFormat, RoundMethod method, QObject *target, const char *slot)
{
    QnRequestParamList params;

    params << QnRequestParam("res_id", camera->getPhysicalId());
    params << QnRequestParam("time", timeUsec);
    if (size.width() > 0)
        params << QnRequestParam("width", size.width());
    if (size.height() > 0)
        params << QnRequestParam("height", size.height());
    params << QnRequestParam("format", imageFormat);
    QString methodStr(lit("before"));
    if (method == Precise)
        methodStr = lit("precise");
    else if (method == IFrameAfterTime)
        methodStr = lit("after");
    params << QnRequestParam("method", methodStr);
    return sendAsyncGetRequest(ImageObject, params, QN_STRINGIZE_TYPE(QImage), target, slot);
}

int QnMediaServerConnection::getTimePeriodsAsync(const QnNetworkResourceList &list, 
                                                 qint64 startTimeMs,
                                                 qint64 endTimeMs, 
                                                 qint64 detail,
                                                 Qn::TimePeriodContent periodsType,
                                                 const QString &filter,
                                                 QObject *target,
                                                 const char *slot) 
{
    QnRequestParamList params;

    foreach(QnNetworkResourcePtr netResource, list)
        params << QnRequestParam("physicalId", netResource->getPhysicalId());
    params << QnRequestParam("startTime", QString::number(startTimeMs));
    params << QnRequestParam("endTime", QString::number(endTimeMs));
    params << QnRequestParam("detail", QString::number(detail));
#ifdef QN_ENABLE_BOOKMARKS
    if (periodsType == Qn::BookmarksContent)
        params << QnRequestParam("format", "bii");
    else
#endif
        params << QnRequestParam("format", "bin");
    params << QnRequestParam("periodsType", QString::number(static_cast<int>(periodsType)));
    params << QnRequestParam("filter", filter);

    return sendAsyncGetRequest(TimePeriodsObject, params, QN_STRINGIZE_TYPE(QnTimePeriodList), target, slot);
}

QnRequestParamList QnMediaServerConnection::createGetParamsRequest(const QnNetworkResourcePtr &camera, const QStringList &params) {
    QnRequestParamList result;
    result << QnRequestParam("res_id", camera->getPhysicalId());
    foreach(QString param, params)
        result << QnRequestParam(param, QString());
    return result;
}

int QnMediaServerConnection::getParamsAsync(const QnNetworkResourcePtr &camera, const QStringList &keys, QObject *target, const char *slot) {
    return sendAsyncGetRequest(GetParamsObject, createGetParamsRequest(camera, keys), QN_STRINGIZE_TYPE(QnStringVariantPairList), target, slot);
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
    return sendAsyncGetRequest(SetParamsObject, createSetParamsRequest(camera, params), QN_STRINGIZE_TYPE(QnStringBoolPairList), target, slot);
}

int QnMediaServerConnection::setParamsSync(const QnNetworkResourcePtr &camera, const QnStringVariantPairList &params, QnStringBoolPairList *reply) {
    return sendSyncGetRequest(SetParamsObject, createSetParamsRequest(camera, params), reply);
}

int QnMediaServerConnection::searchCameraAsyncStart(const QString &startAddr, const QString &endAddr, const QString &username, const QString &password, int port, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("start_ip", startAddr);
    if (!endAddr.isEmpty())
        params << QnRequestParam("end_ip", endAddr);
    params << QnRequestParam("user", username);
    params << QnRequestParam("password", password);
    params << QnRequestParam("port" ,QString::number(port));

    return sendAsyncGetRequest(CameraSearchStartObject, params, QN_STRINGIZE_TYPE(QnManualCameraSearchReply), target, slot);
}

int QnMediaServerConnection::searchCameraAsyncStatus(const QUuid &processUuid, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("uuid", processUuid.toString());
    return sendAsyncGetRequest(CameraSearchStatusObject, params, QN_STRINGIZE_TYPE(QnManualCameraSearchReply), target, slot);
}

int QnMediaServerConnection::searchCameraAsyncStop(const QUuid &processUuid, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("uuid", processUuid.toString());
    return sendAsyncGetRequest(CameraSearchStopObject, params, QN_STRINGIZE_TYPE(QnManualCameraSearchReply), target, slot);
}

int QnMediaServerConnection::addCameraAsync(const QStringList &urls, const QStringList &manufacturers, const QString &username, const QString &password, QObject *target, const char *slot) {
    QnRequestParamList params;
    for (int i = 0; i < qMin(urls.count(), manufacturers.count()); i++){
        params << QnRequestParam(lit("url") + QString::number(i), urls[i]);
        params << QnRequestParam(lit("manufacturer") + QString::number(i), manufacturers[i]);
    }
    params << QnRequestParam("user", username);
    params << QnRequestParam("password", password);

    return sendAsyncGetRequest(CameraAddObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzContinuousMoveAsync(const QnNetworkResourcePtr &camera, const QVector3D &speed, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::ContinuousMovePtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("xSpeed",          QnLexical::serialized(speed.x()));
    params << QnRequestParam("ySpeed",          QnLexical::serialized(speed.y()));
    params << QnRequestParam("zSpeed",          QnLexical::serialized(speed.z()));
    params << QnRequestParam("sequenceId",      QnLexical::serialized(sequenceId));
    params << QnRequestParam("sequenceNumber",  QnLexical::serialized(sequenceNumber));

    return sendAsyncGetRequest(PtzContinuousMoveObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzAbsoluteMoveAsync(const QnNetworkResourcePtr &camera, Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(space == Qn::DevicePtzCoordinateSpace ? Qn::AbsoluteDeviceMovePtzCommand : Qn::AbsoluteLogicalMovePtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("xPos",            QnLexical::serialized(position.x()));
    params << QnRequestParam("yPos",            QnLexical::serialized(position.y()));
    params << QnRequestParam("zPos",            QnLexical::serialized(position.z()));
    params << QnRequestParam("speed",           QnLexical::serialized(speed));
    params << QnRequestParam("sequenceId",      QnLexical::serialized(sequenceId));
    params << QnRequestParam("sequenceNumber",  QnLexical::serialized(sequenceNumber));

    return sendAsyncGetRequest(PtzAbsoluteMoveObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzViewportMoveAsync(const QnNetworkResourcePtr &camera, qreal aspectRatio, const QRectF &viewport, qreal speed, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::ViewportMovePtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("aspectRatio",     QnLexical::serialized(aspectRatio));
    params << QnRequestParam("viewportTop",     QnLexical::serialized(viewport.top()));
    params << QnRequestParam("viewportLeft",    QnLexical::serialized(viewport.left()));
    params << QnRequestParam("viewportBottom",  QnLexical::serialized(viewport.bottom()));
    params << QnRequestParam("viewportRight",   QnLexical::serialized(viewport.right()));
    params << QnRequestParam("speed",           QnLexical::serialized(speed));
    params << QnRequestParam("sequenceId",      QnLexical::serialized(sequenceId));
    params << QnRequestParam("sequenceNumber",  QnLexical::serialized(sequenceNumber));

    return sendAsyncGetRequest(PtzViewportMoveObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzGetPositionAsync(const QnNetworkResourcePtr &camera, Qn::PtzCoordinateSpace space, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(space == Qn::DevicePtzCoordinateSpace ? Qn::GetDevicePositionPtzCommand : Qn::GetLogicalPositionPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));

    return sendAsyncGetRequest(PtzGetPositionObject, params, QN_STRINGIZE_TYPE(QVector3D), target, slot);
}

int QnMediaServerConnection::ptzCreatePresetAsync(const QnNetworkResourcePtr &camera, const QnPtzPreset &preset, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::CreatePresetPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("presetName",      QnLexical::serialized(preset.name));
    params << QnRequestParam("presetId",        QnLexical::serialized(preset.id));

    return sendAsyncGetRequest(PtzCreatePresetObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzUpdatePresetAsync(const QnNetworkResourcePtr &camera, const QnPtzPreset &preset, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::UpdatePresetPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("presetName",      QnLexical::serialized(preset.name));
    params << QnRequestParam("presetId",        QnLexical::serialized(preset.id));

    return sendAsyncGetRequest(PtzUpdatePresetObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzRemovePresetAsync(const QnNetworkResourcePtr &camera, const QString &presetId, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::RemovePresetPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("presetId",        QnLexical::serialized(presetId));

    return sendAsyncGetRequest(PtzRemovePresetObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzActivatePresetAsync(const QnNetworkResourcePtr &camera, const QString &presetId, qreal speed, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::ActivatePresetPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("presetId",        QnLexical::serialized(presetId));
    params << QnRequestParam("speed",           QnLexical::serialized(speed));

    return sendAsyncGetRequest(PtzActivatePresetObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzGetPresetsAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::GetPresetsPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));

    return sendAsyncGetRequest(PtzGetPresetsObject, params, QN_STRINGIZE_TYPE(QnPtzPresetList), target, slot);
}

int QnMediaServerConnection::ptzCreateTourAsync(const QnNetworkResourcePtr &camera, const QnPtzTour &tour, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",          QnLexical::serialized(Qn::CreateTourPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));

    QnRequestHeaderList headers;
    headers << QnRequestParam("content-type",   "application/json");

    return sendAsyncPostRequest(PtzCreateTourObject, headers, params, QJson::serialized(tour), NULL, target, slot);
}

int QnMediaServerConnection::ptzRemoveTourAsync(const QnNetworkResourcePtr &camera, const QString &tourId, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::RemoveTourPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("tourId",          QnLexical::serialized(tourId));

    return sendAsyncGetRequest(PtzRemoveTourObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzActivateTourAsync(const QnNetworkResourcePtr &camera, const QString &tourId, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::ActivateTourPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("tourId",          QnLexical::serialized(tourId));

    return sendAsyncGetRequest(PtzActivateTourObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzGetToursAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::GetToursPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));

    return sendAsyncGetRequest(PtzGetToursObject, params, QN_STRINGIZE_TYPE(QnPtzTourList), target, slot);
}

int QnMediaServerConnection::ptzGetActiveObjectAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::GetActiveObjectPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));

    return sendAsyncGetRequest(PtzGetActiveObjectObject, params, QN_STRINGIZE_TYPE(QnPtzObject), target, slot);
}

int QnMediaServerConnection::ptzUpdateHomeObjectAsync(const QnNetworkResourcePtr &camera, const QnPtzObject &homePosition, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::UpdateHomeObjectPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("objectType",      QnLexical::serialized(homePosition.type));
    params << QnRequestParam("objectId",        QnLexical::serialized(homePosition.id));

    return sendAsyncGetRequest(PtzUpdateHomeObjectObject, params, QN_STRINGIZE_TYPE(QnPtzObject), target, slot);
}

int QnMediaServerConnection::ptzGetHomeObjectAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::GetHomeObjectPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));

    return sendAsyncGetRequest(PtzGetHomeObjectObject, params, QN_STRINGIZE_TYPE(QnPtzObject), target, slot);
}

int QnMediaServerConnection::ptzGetDataAsync(const QnNetworkResourcePtr &camera, Qn::PtzDataFields query, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::GetDataPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("query",           QnLexical::serialized(query));

    return sendAsyncGetRequest(PtzGetDataObject, params, QN_STRINGIZE_TYPE(QnPtzData), target, slot);
}

int QnMediaServerConnection::getTimeAsync(QObject *target, const char *slot) {
    return sendAsyncGetRequest(TimeObject, QnRequestParamList(), QN_STRINGIZE_TYPE(QnTimeReply), target, slot);
}

int QnMediaServerConnection::doCameraDiagnosticsStepAsync(
    const QnId& cameraID, CameraDiagnostics::Step::Value previousStep,
    QObject* target, const char* slot )
{
    QnRequestParamList params;
    params << QnRequestParam("res_id",  cameraID);
    params << QnRequestParam("type", CameraDiagnostics::Step::toString(previousStep));
    return sendAsyncGetRequest(CameraDiagnosticsObject, params, QN_STRINGIZE_TYPE(QnCameraDiagnosticsReply), target, slot);
}

int QnMediaServerConnection::doRebuildArchiveAsync(RebuildAction action, QObject *target, const char *slot)
{
    QnRequestParamList params;
    if (action == RebuildAction_Start)
        params << QnRequestParam("action",  lit("start"));
    else if (action == RebuildAction_Cancel)
        params << QnRequestParam("action",  lit("stop"));
    return sendAsyncGetRequest(RebuildArchiveObject, params, QN_STRINGIZE_TYPE(QnRebuildArchiveReply), target, slot);
}

int QnMediaServerConnection::getStorageSpaceAsync(QObject *target, const char *slot) {
    return sendAsyncGetRequest(StorageSpaceObject, QnRequestParamList(), QN_STRINGIZE_TYPE(QnStorageSpaceReply), target, slot);
}

int QnMediaServerConnection::getStorageStatusAsync(const QString &storageUrl, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("path", storageUrl);

    return sendAsyncGetRequest(StorageStatusObject, params, QN_STRINGIZE_TYPE(QnStorageStatusReply), target, slot);
}

int QnMediaServerConnection::getStatisticsAsync(QObject *target, const char *slot){
    return sendAsyncGetRequest(StatisticsObject, QnRequestParamList(), QN_STRINGIZE_TYPE(QnStatisticsReply), target, slot);
}

int QnMediaServerConnection::getEventLogAsync(
                  qint64 dateFrom, qint64 dateTo,
                  QnResourceList camList,
                  QnBusiness::EventType eventType,
                  QnBusiness::ActionType actionType,
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
    if (!businessRuleId.isNull())
        params << QnRequestParam( "brule_id", businessRuleId );
    if (eventType != QnBusiness::UndefinedEvent)
        params << QnRequestParam( "event", (int) eventType);
    if (actionType != QnBusiness::UndefinedAction)
        params << QnRequestParam( "action", (int) actionType);

    return sendAsyncGetRequest(EventLogObject, params, QN_STRINGIZE_TYPE(QnBusinessActionDataListPtr), target, slot);
}

int QnMediaServerConnection::addBookmarkAsync(const QnNetworkResourcePtr &camera, const QnCameraBookmark &bookmark, QObject *target, const char *slot) {
    QnRequestHeaderList headers;
    headers << QnRequestParam("content-type",   "application/json");

    QnRequestParamList params;
    params << QnRequestParam("id",      QnLexical::serialized(camera->getPhysicalId()));

    return sendAsyncPostRequest(BookmarkAddObject, headers, params, QJson::serialized(bookmark), QN_STRINGIZE_TYPE(QnCameraBookmark), target, slot);
}

int QnMediaServerConnection::updateBookmarkAsync(const QnNetworkResourcePtr &camera, const QnCameraBookmark &bookmark, QObject *target, const char *slot) {
    QnRequestHeaderList headers;
    headers << QnRequestParam("content-type",   "application/json");

    QnRequestParamList params;
    params << QnRequestParam("id",      QnLexical::serialized(camera->getPhysicalId()));

    return sendAsyncPostRequest(BookmarkUpdateObject, headers, params, QJson::serialized(bookmark), QN_STRINGIZE_TYPE(QnCameraBookmark), target, slot);
}

int QnMediaServerConnection::deleteBookmarkAsync(const QnNetworkResourcePtr &camera, const QnCameraBookmark &bookmark, QObject *target, const char *slot) {
    QnRequestHeaderList headers;
    headers << QnRequestParam("content-type",   "application/json");

    QnRequestParamList params;
    params << QnRequestParam("id",      QnLexical::serialized(camera->getPhysicalId()));

    return sendAsyncPostRequest(BookmarkDeleteObject, headers, params, QJson::serialized(bookmark), QN_STRINGIZE_TYPE(QnCameraBookmark), target, slot);
}

int QnMediaServerConnection::getBookmarksAsync(const QnNetworkResourcePtr &camera, const QnCameraBookmarkSearchFilter &filter, QObject *target, const char *slot) {
    QnRequestHeaderList headers;
    headers << QnRequestParam("content-type",   "application/json");

    QnRequestParamList params;
    params << QnRequestParam("id",               QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("minStartTimeMs",   QnLexical::serialized(filter.minStartTimeMs));
    params << QnRequestParam("maxStartTimeMs",   QnLexical::serialized(filter.maxStartTimeMs));
    params << QnRequestParam("minDurationMs",    QnLexical::serialized(filter.minDurationMs));
    params << QnRequestParam("text",             QnLexical::serialized(filter.text));
    
    return sendAsyncGetRequest(BookmarksGetObject, headers, params, QN_STRINGIZE_TYPE(QnCameraBookmarkList), target, slot);
}

int QnMediaServerConnection::changeSystemNameAsync(const QString &systemName, bool reboot, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("systemName", systemName);
    if (reboot)
        params << QnRequestParam("reboot", true);

    return sendAsyncGetRequest(ChangeSystemNameObject, params, NULL, target, slot);
}

