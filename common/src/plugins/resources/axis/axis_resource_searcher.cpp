#ifdef ENABLE_AXIS

#include "axis_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "axis_resource.h"

extern QString getValueFromString(const QString& line);

QnPlAxisResourceSearcher::QnPlAxisResourceSearcher()
{
}

QnPlAxisResourceSearcher& QnPlAxisResourceSearcher::instance()
{
    static QnPlAxisResourceSearcher inst;
    return inst;
}

QnResourcePtr QnPlAxisResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
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

    result = QnVirtualCameraResourcePtr( new QnPlAxisResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create Axis camera resource. TypeID" << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;

}

QString QnPlAxisResourceSearcher::manufacture() const
{
    return QLatin1String(QnPlAxisResource::MANUFACTURE);
}


QList<QnResourcePtr> QnPlAxisResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool isSearchAction)
{
    if( !url.scheme().isEmpty() && isSearchAction )
        return QList<QnResourcePtr>();  //searching if only host is present, not specific protocol

    QString host = url.host();
    int port = url.port();
    if (host.isEmpty())
        host = url.toString(); // in case if url just host address without protocol and port

    int timeout = 4000;


    if (port < 0)
        port = 80;

    CLHttpStatus status;
    QString response = QString(QLatin1String(downloadFile(status, QLatin1String("axis-cgi/param.cgi?action=list&group=Network"), host, port, timeout, auth)));

    if (response.length()==0)
        return QList<QnResourcePtr>();

    QStringList lines = response.split(QLatin1String("\n"), QString::SkipEmptyParts);


    QString name;
    QString mac;

    foreach(QString line, lines)
    {
        if (line.contains(QLatin1String("root.Network.Bonjour.FriendlyName")))
        {
            name = getValueFromString(line);
            name.replace(QLatin1Char(' '), QString());
        }
        else if (line.contains(QLatin1String("root.Network.eth0.MACAddress")))
        {
            mac = getValueFromString(line);
            mac.replace(QLatin1Char(':'),QLatin1Char('-'));
        }

    }

    name = name.left(name.lastIndexOf(QLatin1Char('-')));
    name.replace(QLatin1Char('-'), QString());
    

    if (mac.isEmpty() || name.isEmpty())
        return QList<QnResourcePtr>();


    QnId typeId = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (!typeId.isValid())
        return QList<QnResourcePtr>();

    QnPlAxisResourcePtr resource(new QnPlAxisResource());
    resource->setTypeId(typeId);
    resource->setName(name);
    resource->setModel(name);
    resource->setMAC(mac);
    //resource->setHostAddress(host, QnDomainMemory);
    QUrl finalUrl(url);
    finalUrl.setScheme(QLatin1String("http"));
    finalUrl.setPort(port);
    resource->setUrl(finalUrl.toString());
    resource->setAuth(auth);

    //resource->setDiscoveryAddr(iface.address);
    QList<QnResourcePtr> result;
    result << resource;
    return result;
}

QList<QnNetworkResourcePtr> QnPlAxisResourceSearcher::processPacket(QnResourceList& result, const QByteArray& responseData, const QHostAddress& discoveryAddress)
{

    QString smac;
    QString name;

    QList<QnNetworkResourcePtr> local_results;

    int iqpos = responseData.indexOf("AXIS");


    if (iqpos<0)
        return local_results;

    int macpos = responseData.indexOf("- 00", iqpos);
    if (macpos < 0)
        return local_results;
    macpos += 2;

    for (int i = iqpos; i < macpos; i++)
    {
        name += QLatin1Char(responseData[i]);
    }

    int macpos2 = responseData.indexOf("axis-00", macpos);
    if (macpos2 > 0)
        macpos = macpos2 + 5; // replace real MAC to virtual MAC if exists

    name.replace(QLatin1Char(' '), QString()); // remove spaces
    name.replace(QLatin1Char('-'), QString()); // remove spaces
    name.replace(QLatin1Char('\t'), QString()); // remove tabs

    if (macpos+12 > responseData.size())
        return local_results;


    //macpos++; // -

    while(responseData.at(macpos)==' ')
        ++macpos;


    if (macpos+12 > responseData.size())
        return local_results;



    for (int i = 0; i < 12; i++)
    {
        if (i > 0 && i % 2 == 0)
            smac += QLatin1Char('-');

        smac += QLatin1Char(responseData[macpos + i]);
    }


    //response.fromDatagram(responseData);

    smac = smac.toUpper();

    foreach(QnResourcePtr res, result)
    {
        QnNetworkResourcePtr net_res = res.dynamicCast<QnNetworkResource>();
    
        if (net_res->getMAC().toString() == smac) {
            if (isNewDiscoveryAddressBetter(net_res->getHostAddress(), discoveryAddress.toString(), net_res->getDiscoveryAddr().toString()))
                net_res->setDiscoveryAddr(discoveryAddress);
            return local_results; // already found;
        }
    }


    QnPlAxisResourcePtr resource ( new QnPlAxisResource() );

    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (!rt.isValid())
        return local_results;

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setModel(name);
    resource->setMAC(smac);

    local_results.push_back(resource);


    

    int channesl = 1;

    if (resource->hasParam(QLatin1String("channelsAmount")))
    {
        QVariant val;
        resource->getParam(QLatin1String("channelsAmount"), val, QnDomainMemory);
        channesl = val.toUInt();
    }

    if (channesl > 1) //
    {
        resource->setGroupName(resource->getPhysicalId());
        resource->setGroupId(resource->getPhysicalId());

        resource->setPhysicalId(resource->getPhysicalId() + QLatin1String("_channel_") + QString::number(1));

        for (int i = 2; i <= channesl; ++i)
        {
            QnPlAxisResourcePtr resource ( new QnPlAxisResource() );

            QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
            if (!rt.isValid())
                return local_results;

            resource->setTypeId(rt);
            resource->setName(name);
            resource->setModel(name);
            resource->setMAC(smac);
            resource->setGroupName(resource->getPhysicalId());
            resource->setGroupId(resource->getPhysicalId());

            resource->setPhysicalId(resource->getPhysicalId() + QLatin1String("_channel_") + QString::number(i));

            local_results.push_back(resource);

        }
    }


    return local_results;
}

#endif // #ifdef ENABLE_AXIS
