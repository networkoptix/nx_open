#include <QNetworkReply>

#include "acti_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "acti_resource.h"
#include "../mdns/mdns_listener.h"

extern QString getValueFromString(const QString& line);

static const QString DEFAULT_LOGIN(QLatin1String("admin"));
static const QString DEFAULT_PASSWORD(QLatin1String("123456"));
static const int ACTI_DEVICEXML_PORT = 49152;
static const int CACHE_UPDATE_TIME = 60 * 1000;

QnActiResourceSearcher::QnActiResourceSearcher():
    QObject(), QnUpnpResourceSearcher()
{
    QnMdnsListener::instance()->registerConsumer((long) this);
}

QnActiResourceSearcher::~QnActiResourceSearcher()
{
    delete m_manager;
}

QnResourceList QnActiResourceSearcher::findResources(void)
{
    // find via pnp
    QnResourceList result = QnUpnpResourceSearcher::findResources();

    // find via MDNS
    QList<QByteArray> processedUuid;
    QnMdnsListener::ConsumerDataList data = QnMdnsListener::instance()->getData((long) this);
    for (int i = 0; i < data.size(); ++i)
    {
        if (shouldStop())
            break;

        QString removeAddress = data[i].remoteAddress;
        QByteArray uuidStr("ACTI");
        uuidStr += data[i].remoteAddress.toUtf8();
        QByteArray response = data[i].response;

        if (response.contains("_http") && response.contains("_tcp") && response.contains("local"))
        {
            if (processedUuid.contains(uuidStr))
                continue;

            QByteArray response = getDeviceXml(QString(QLatin1String("http://%1:%2/devicedesc.xml")).arg(removeAddress).arg(ACTI_DEVICEXML_PORT)); // async request
            //QByteArray response = getDeviceXml(QString(QLatin1String("http://%1:%2")).arg(removeAddress).arg(80)); // test request
            processDeviceXml(response, removeAddress, removeAddress, result);
            processedUuid << uuidStr;
        }
    }
    
    return result;
}

QByteArray QnActiResourceSearcher::getDeviceXml(const QUrl& url)
{
    QMutexLocker lock(&m_mutex);

    QString host = url.host();
    CasheInfo info = m_cachedXml.value(host);
    if (info.xml.isEmpty() || info.timer.elapsed() > CACHE_UPDATE_TIME)
    {
        if (!m_httpInProgress.contains(url.host())) 
        {
            if (!m_manager) {
                m_manager = new QNetworkAccessManager();
                connect(m_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(at_replyReceived(QNetworkReply*)));
            }
            m_manager->get(QNetworkRequest(url));
            m_httpInProgress << url.host();
        }
    }

    return m_cachedXml.value(host).xml;
}

void QnActiResourceSearcher::at_replyReceived(QNetworkReply* reply)
{
    QMutexLocker lock(&m_mutex);

    QString host = reply->url().host();
    m_cachedXml[host].xml = reply->readAll();
    m_cachedXml[host].timer.restart();
    m_httpInProgress.remove(host);

    reply->deleteLater();
}

QnActiResourceSearcher& QnActiResourceSearcher::instance()
{
    static QnActiResourceSearcher inst;
    return inst;
}

QnResourcePtr QnActiResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        qDebug() << "No resource type for ID = " << resourceTypeId;

        return result;
    }

    if (resourceType->getManufacture() != manufacture())
    {
        //qDebug() << "Manufature " << resourceType->getManufacture() << " != " << manufacture();
        return result;
    }

    result = QnVirtualCameraResourcePtr( new QnActiResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create ACTI camera resource. TypeID" << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;

}

QString QnActiResourceSearcher::manufacture() const
{
    return QLatin1String(QnActiResource::MANUFACTURE);
}


QList<QnResourcePtr> QnActiResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    Q_UNUSED(doMultichannelCheck)

    QnResourceList result;

    QByteArray uuidStr("ACTI");
    uuidStr += url.host().toUtf8();
    readDeviceXml(uuidStr, QString(QLatin1String("http://%1:%2/devicedesc.xml")).arg(url.host()).arg(ACTI_DEVICEXML_PORT), url.host(), result);
    foreach(QnResourcePtr res, result)
        res.dynamicCast<QnNetworkResource>()->setAuth(auth);

    return result;
}

void QnActiResourceSearcher::processPacket(
    const QHostAddress& discoveryAddr,
    const QString& host,
    const UpnpDeviceInfo& devInfo,
    const QByteArray& /*xmlDevInfo*/,
    QnResourceList& result)
{
    Q_UNUSED(discoveryAddr)
    Q_UNUSED(host)
    if (!devInfo.manufacturer.toUpper().startsWith(manufacture()))
        return;

    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), QLatin1String("ACTI_COMMON"));
    if (!rt.isValid())
        return;

    QnActiResourcePtr resource ( new QnActiResource() );

    resource->setTypeId(rt);
    resource->setName(QString(QLatin1String("ACTi-")) + devInfo.modelName);
    resource->setModel(devInfo.modelName);
    resource->setUrl(devInfo.presentationUrl);
    resource->setPhysicalId(QString(QLatin1String("ACTI_%1")).arg(devInfo.serialNumber));
    QAuthenticator auth;
    
    auth.setUser(DEFAULT_LOGIN);
    auth.setPassword(DEFAULT_PASSWORD);
    resource->setAuth(auth);

    result << resource;
}
