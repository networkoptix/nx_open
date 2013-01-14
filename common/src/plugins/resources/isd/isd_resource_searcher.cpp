#include "core/resource/camera_resource.h"
#include "isd_resource_searcher.h"
#include "isd_resource.h"

extern QString getValueFromString(const QString& line);

QnPlISDResourceSearcher::QnPlISDResourceSearcher()
{
}

QnPlISDResourceSearcher& QnPlISDResourceSearcher::instance()
{
    static QnPlISDResourceSearcher inst;
    return inst;
}

QnResourcePtr QnPlISDResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
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

    result = QnVirtualCameraResourcePtr( new QnPlIsdResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create ISD camera resource. typeID:" << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;

}

QString QnPlISDResourceSearcher::manufacture() const
{
    return QLatin1String(QnPlIsdResource::MANUFACTURE);
}


QList<QnResourcePtr> QnPlISDResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{

    QString host = url.host();
    int port = url.port();
    if (host.isEmpty())
        host = url.toString(); // in case if url just host address without protocol and port

    int timeout = 2000;


    if (port < 0)
        port = 80;

    CLHttpStatus status;
    QString name = QString(QLatin1String(downloadFile(status, QLatin1String("api/param.cgi?req=General.Brand.ModelName"), host, port, timeout, auth)));

    name.replace(QLatin1Char(' '), QString()); // remove spaces
    //name.replace(QLatin1Char('-'), QString()); // remove spaces
    name.replace(QLatin1Char('\r'), QString()); // remove spaces
    name.replace(QLatin1Char('\n'), QString()); // remove spaces
    name.replace(QLatin1Char('\t'), QString()); // remove tabs


    if (name.length()==0)
        return QList<QnResourcePtr>();


    name = getValueFromString(name).trimmed();



    if (name.endsWith(QLatin1Char('0')))
        name.chop(1);


    QString mac = QString(QLatin1String(downloadFile(status, QLatin1String("/api/param.cgi?req=Network.1.MacAddress"), host, port, timeout, auth)));

    mac.replace(QLatin1Char(' '), QString()); // remove spaces
    mac.replace(QLatin1Char('\r'), QString()); // remove spaces
    mac.replace(QLatin1Char('\n'), QString()); // remove spaces
    mac.replace(QLatin1Char('\t'), QString()); // remove tabs


    if (mac.isEmpty() || name.isEmpty())
        return QList<QnResourcePtr>();


    mac = getValueFromString(mac).trimmed();

    //int n = mac.length();

    if (mac.length() > 17 && mac.endsWith(QLatin1Char('0')))
        mac.chop(mac.length() - 17);





    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (!rt.isValid())
        return QList<QnResourcePtr>();

    QnPlIsdResourcePtr resource ( new QnPlIsdResource() );

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setModel(name);
    resource->setMAC(mac);
    resource->setHostAddress(host, QnDomainMemory);
    resource->setAuth(auth);

    //resource->setDiscoveryAddr(iface.address);
    QList<QnResourcePtr> result;
    result << resource;
    return result;
}

QList<QnNetworkResourcePtr> QnPlISDResourceSearcher::processPacket(QnResourceList& result, QByteArray& responseData, const QHostAddress& discoveryAddress)
{

    QList<QnNetworkResourcePtr> local_result;


    QString smac;
    QString name;

    int iqpos = responseData.indexOf("ISD");


    if (iqpos<0)
        return local_result;

    int macpos = responseData.indexOf("macaddress=");
    if (macpos < 0)
        return local_result;

    macpos += QString(QLatin1String("macaddress=")).length();
    if (macpos + 12 > responseData.size())
        return local_result;


    //name = responseData.mid();
    name = QLatin1String("ISDcam");
//
//    for (int i = iqpos; i < macpos; i++)
//    {
//        name += QLatin1Char(responseData[i]);
//    }
//
    name.replace(QLatin1Char(' '), QString()); // remove spaces
    name.replace(QLatin1Char('-'), QString()); // remove spaces
    name.replace(QLatin1Char('\t'), QString()); // remove tabs

    //macpos++; // -
//
//    while(responseData.at(macpos)==' ')
//        ++macpos;
//
//
//    if (macpos+12 > responseData.size())
//        return QnNetworkResourcePtr(0);
//


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
    
        if (net_res->getMAC().toString() == smac)
        {
            if (isNewDiscoveryAddressBetter(net_res->getHostAddress(), discoveryAddress.toString(), net_res->getDiscoveryAddr().toString()))
                net_res->setDiscoveryAddr(discoveryAddress);
            return local_result; // already found;
        }
    }


    QnPlIsdResourcePtr resource ( new QnPlIsdResource() );

    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (!rt.isValid())
    {
        return local_result;
    }

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setModel(name);
    resource->setMAC(smac);

    local_result.push_back(resource);

    return local_result;


}
