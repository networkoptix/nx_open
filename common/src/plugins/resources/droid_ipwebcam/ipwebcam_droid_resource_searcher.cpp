#include "core/resource/camera_resource.h"
#include "ipwebcam_droid_resource_searcher.h"
#include "ipwebcam_droid_resource.h"
#include "core/resourcemanagment/resource_pool.h"



QnPlIpWebCamResourceSearcher::QnPlIpWebCamResourceSearcher()
{
}

QnPlIpWebCamResourceSearcher& QnPlIpWebCamResourceSearcher::instance()
{
    static QnPlIpWebCamResourceSearcher inst;
    return inst;
}

QnResourcePtr QnPlIpWebCamResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
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

    if (parameters.value("url").contains("raw://"))
    {
        return result; // it is new droid resource
    }

    result = QnVirtualCameraResourcePtr( new QnPlDriodIpWebCamResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create ipWEB camera resource. typeID:" << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;

}

QString QnPlIpWebCamResourceSearcher::manufacture() const
{
    return QnPlDriodIpWebCamResource::MANUFACTURE;
}


QnResourcePtr QnPlIpWebCamResourceSearcher::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr(0);
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

        QString t = QHostAddress(ip).toString();
        if (t=="192.168.1.135")
            t = t;

        CLSimpleHTTPClient httpClient(QHostAddress(ip), 8080, 2000, QAuthenticator());
        httpClient.doGET(request);

        if (httpClient.isOpened())
        {
            android = true;
            localAddr = httpClient.getLocalHost().toString();
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

        QString name = "DroidLive";

        foreach(AnDroidDev ad, alist)
        {
            if (ad.android)
            {

                QnNetworkResourcePtr resource ( new QnPlDriodIpWebCamResource() );

                QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
                if (!rt.isValid())
                    continue;

                static int n = 0;
                n++;

                QString lastMac = QString::number(n, 16).toUpper();
                if (lastMac.length()<2)
                    lastMac = QString("0") + lastMac;

                QString smac = QString("00-00-00-00-00-") + lastMac;


                resource->setTypeId(rt);
                resource->setName(name);
                resource->setMAC(smac);
                resource->setHostAddress(QHostAddress(ad.ip), QnDomainMemory);
                
                resource->setDiscoveryAddr(QHostAddress(ad.localAddr));

                result.push_back(resource);


            }
        }

    }

    return result;
}