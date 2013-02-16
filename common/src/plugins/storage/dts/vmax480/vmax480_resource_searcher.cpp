#include "vmax480_resource_searcher.h"
#include "vmax480_resource.h"

struct Vmax480Box
{
    QString ip;
    QString login;
    QString pass;

    int videoPort;
    int eventPort;
};

//====================================================================
QnPlVmax480ResourceSearcher::QnPlVmax480ResourceSearcher()
{

}

QnPlVmax480ResourceSearcher& QnPlVmax480ResourceSearcher::instance()
{
    static QnPlVmax480ResourceSearcher inst;
    return inst;
}

QnResourceList QnPlVmax480ResourceSearcher::findResources(void)
{
    QnResourceList result;

    QFile file(QLatin1String("c:/vmax.txt")); // Create a file handle for the file named
    if (!file.exists())
        return result;

    QString line;

    if (!file.open(QIODevice::ReadOnly)) // Open the file
        return result;

    QTextStream stream(&file); // Set the stream to read from myFile

    QnInterfaceAndAddr iface = getAllIPv4Interfaces().first();

    while(1)
    {
        line = stream.readLine(); // this reads a line (QString) from the file

        if (line.trimmed().isEmpty())
            break;

        QStringList params = line.split(QLatin1Char(';'), QString::SkipEmptyParts);

        QnPlVmax480ResourcePtr resource ( new QnPlVmax480Resource() );

        QString name = params[0];
        QString mac =  params[1];
        QString host =  params[2];
        QString port =  params[3];
        QAuthenticator auth;
        auth.setUser(QLatin1String("admin"));

        QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);

        resource->setTypeId(rt);
        resource->setName(name);
        (resource.dynamicCast<QnPlVmax480Resource>())->setModel(name);
        resource->setMAC(mac);
        resource->setUrl(QString(QLatin1String("http://%1:%2")).arg(host).arg(port));
        resource->setDiscoveryAddr(iface.address);
        resource->setAuth(auth);

        //resource->setDiscoveryAddr(iface.address);
        QList<QnResourcePtr> result;
        result << resource;
        return result;

    }

    return result;
}

QnResourcePtr QnPlVmax480ResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
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

    result = QnVirtualCameraResourcePtr( new QnPlVmax480Resource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create test camera resource. typeID:" << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;

}

QList<QnResourcePtr> QnPlVmax480ResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    QList<QnResourcePtr> result;
    return result;
}

QString QnPlVmax480ResourceSearcher::manufacture() const
{
    return QLatin1String(QnPlVmax480Resource::MANUFACTURE);
}
