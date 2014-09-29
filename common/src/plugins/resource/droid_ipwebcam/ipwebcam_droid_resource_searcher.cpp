#ifdef ENABLE_DROID

#include "core/resource/camera_resource.h"
#include "ipwebcam_droid_resource_searcher.h"
#include "ipwebcam_droid_resource.h"
#include "core/resource_management/resource_pool.h"

#include <QtConcurrent/QtConcurrentMap>



QnPlIpWebCamResourceSearcher::QnPlIpWebCamResourceSearcher()
{
}

QnResourcePtr QnPlIpWebCamResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& params)
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

    if (params.url.contains(lit("raw://")))
    {
        return result; // it is new droid resource
    }

    result = QnVirtualCameraResourcePtr( new QnPlDriodIpWebCamResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create ipWEB camera resource. typeID:" << resourceTypeId.toString(); // << ", Parameters: " << parameters;
    //result->deserialize(parameters);

    return result;

}

QString QnPlIpWebCamResourceSearcher::manufacture() const
{
    return QnPlDriodIpWebCamResource::MANUFACTURE;
}


QList<QnResourcePtr> QnPlIpWebCamResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    Q_UNUSED(url)
    Q_UNUSED(auth)
    Q_UNUSED(doMultichannelCheck)
    return QList<QnResourcePtr>();
}


struct AnDroidDev
{
    // Aluma - All I Need Is Time (Sluslik Luna Mix)

    quint32 ip;
    bool android;
    QString localAddr;

    void checkIfItAndroid()
    {
        android = false;
        QString request;

        std::auto_ptr<AbstractStreamSocket> sock( SocketFactory::createStreamSocket() );
        sock->setRecvTimeout(500);
        sock->setSendTimeout(500);

        if (sock->connect(QHostAddress(ip).toString(), 8080, AbstractCommunicatingSocket::DEFAULT_TIMEOUT_MILLIS))
        {
            android = true;
            
            localAddr = sock->getLocalAddress().address.toString();
        }

    }
};


QnResourceList QnPlIpWebCamResourceSearcher::findResources()
{
    QnResourceList result;



    QFile file(QLatin1String("android.txt")); // Create a file handle for the file named
    if (!file.exists())
        return result;

    QString line;

    if (!file.open(QIODevice::ReadOnly)) // Open the file
        return result;

    QTextStream stream(&file); // Set the stream to read from myFile

    while(1)
    {
        line = stream.readLine(); // this reads a line (QString) from the file

        if (line.trimmed().isEmpty())
            break;

        QStringList list = line.split(QLatin1Char(' '), QString::SkipEmptyParts);


        if (list.count()<2)
            continue;

        QHostAddress min_addr(list.at(0));
        QHostAddress max_addr(list.at(1));


        QList<AnDroidDev> alist;

        quint32 curr = min_addr.toIPv4Address();
        quint32 max_ip = max_addr.toIPv4Address();

        while(curr <= max_ip)
        {
            QnResourcePtr nd = qnResPool->getResourceByUrl(QHostAddress(curr).toString());
            if (nd)
            {
                // such dev alredy exists;
                ++curr;
                continue;
            }

            AnDroidDev adev;
            adev.ip = curr;
            alist.push_back(adev);
            ++curr;
        }


        int threads = 10;
        QThreadPool* global = QThreadPool::globalInstance();
        for (int i = 0; i < threads; ++i ) global->releaseThread();
        QtConcurrent::blockingMap(alist, &AnDroidDev::checkIfItAndroid);
        for (int i = 0; i < threads; ++i )global->reserveThread();

        QString name = QLatin1String("DroidLive");

        foreach(AnDroidDev ad, alist)
        {
            if (ad.android)
            {

                QnNetworkResourcePtr resource ( new QnPlDriodIpWebCamResource() );

                QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), name);
                if (rt.isNull())
                    continue;

                static int n = 0;
                n++;

                QString lastMac = QString::number(n, 16).toUpper();
                if (lastMac.length()<2)
                    lastMac = QLatin1String("0") + lastMac;

                QString smac = QLatin1String("00-00-00-00-00-") + lastMac;


                resource->setTypeId(rt);
                resource->setName(name);
                resource->setMAC(QnMacAddress(smac));
                resource->setHostAddress(QHostAddress(ad.ip).toString(), QnDomainMemory);
                
                resource->setDiscoveryAddr(QHostAddress(ad.localAddr));

                result.push_back(resource);


            }
        }

    }

    return result;
}

#endif // #ifdef ENABLE_DROID
