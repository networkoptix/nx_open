#include "acti_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "acti_resource.h"
#include "../mdns/mdns_listener.h"

extern QString getValueFromString(const QString& line);

static const QString DEFAULT_LOGIN(QLatin1String("admin"));
static const QString DEFAULT_PASSWORD(QLatin1String("123456"));

QnActiResourceSearcher::QnActiResourceSearcher()
{
    QnMdnsListener::instance()->registerConsumer((long) this);
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
        QString removeAddress = data[i].remoteAddress;
        QByteArray uuidStr("ACTI");
        uuidStr += data[i].remoteAddress.toUtf8();
        QByteArray response = data[i].response;

        if (response.contains("_http") && response.contains("_tcp") && response.contains("local"))
        {
            if (processedUuid.contains(uuidStr))
                continue;

            processDeviceXml(uuidStr, QString(QLatin1String("http://%1:49152/devicedesc.xml")).arg(removeAddress), removeAddress, result);
            processedUuid << uuidStr;
        }
    }
    return result;
}

QnActiResourceSearcher& QnActiResourceSearcher::instance()
{
    static QnActiResourceSearcher inst;
    return inst;
}

QnActiResourceSearcher::~QnActiResourceSearcher()
{
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

    return QList<QnResourcePtr>();
}

void QnActiResourceSearcher::processPacket(const QHostAddress& discoveryAddr, const QString& host, const UpnpDeviceInfo& devInfo, QnResourceList& result)
{
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
