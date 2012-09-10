#include "core/resource/camera_resource.h"
#include "isd_resource_searcher.h"
#include "isd_resource.h"


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


QnResourcePtr QnPlISDResourceSearcher::checkHostAddr(QHostAddress addr)
{
    Q_UNUSED(addr)
    return QnResourcePtr(0);
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
            if (isNewDiscoveryAddressBetter(net_res->getHostAddress().toString(), discoveryAddress.toString(), net_res->getDiscoveryAddr().toString()))
                net_res->setDiscoveryAddr(discoveryAddress);
            return local_result; // already found;
        }
    }


    QnNetworkResourcePtr resource ( new QnPlIsdResource() );

    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (!rt.isValid())
    {
        return local_result;
    }

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setMAC(smac);

    local_result.push_back(resource);

    return local_result;


}
