#include "axis_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "axis_resource.h"

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


QnResourcePtr QnPlAxisResourceSearcher::checkHostAddr(QHostAddress addr)
{
    Q_UNUSED(addr)
    return QnResourcePtr(0);
}

QList<QnNetworkResourcePtr> QnPlAxisResourceSearcher::processPacket(QnResourceList& result, QByteArray& responseData, const QHostAddress& discoveryAddress)
{

    QString smac;
    QString name;

    QList<QnNetworkResourcePtr> local_results;

    int iqpos = responseData.indexOf("AXIS");


    if (iqpos<0)
        return local_results;

    int macpos = responseData.indexOf("00", iqpos);
    if (macpos < 0)
        return local_results;

    for (int i = iqpos; i < macpos; i++)
    {
        name += QLatin1Char(responseData[i]);
    }

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
            if (isNewDiscoveryAddressBetter(net_res->getHostAddress().toString(), discoveryAddress.toString(), net_res->getDiscoveryAddr().toString()))
                net_res->setDiscoveryAddr(discoveryAddress);
            return local_results; // already found;
        }
    }


    QnNetworkResourcePtr resource ( new QnPlAxisResource() );

    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (!rt.isValid())
        return local_results;

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setMAC(smac);

    local_results.push_back(resource);


    

    int channesl = 1;

    if (resource->hasSuchParam(QLatin1String("channelsAmount")))
    {
        QVariant val;
        resource->getParam(QLatin1String("channelsAmount"), val, QnDomainMemory);
        channesl = val.toUInt();
    }

    if (channesl > 1) //
    {

        resource->setPhysicalId(resource->getPhysicalId() + QLatin1String("_channel_") + QString::number(1));

        for (int i = 2; i <= channesl; ++i)
        {
            QnNetworkResourcePtr resource ( new QnPlAxisResource() );

            QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
            if (!rt.isValid())
                return local_results;

            resource->setTypeId(rt);
            resource->setName(name);
            resource->setMAC(smac);

            resource->setPhysicalId(resource->getPhysicalId() + QLatin1String("_channel_") + QString::number(i));

            local_results.push_back(resource);

        }
    }


    return local_results;
}
