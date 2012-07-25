#include "core/resource/camera_resource.h"
#include "iqinvision_resource_searcher.h"
#include "iqinvision_resource.h"


QnPlIqResourceSearcher::QnPlIqResourceSearcher()
{
}

QnPlIqResourceSearcher& QnPlIqResourceSearcher::instance()
{
    static QnPlIqResourceSearcher inst;
    return inst;
}

QnResourcePtr QnPlIqResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
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

    result = QnVirtualCameraResourcePtr( new QnPlIqResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create IQE camera resource. typeID:" << resourceTypeId.toString() << ", Parameters: " << parameters;

    result->deserialize(parameters);

    return result;

}

QString QnPlIqResourceSearcher::manufacture() const
{
    return QLatin1String(QnPlIqResource::MANUFACTURE);
}


QnResourcePtr QnPlIqResourceSearcher::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr(0);
}

QList<QnNetworkResourcePtr> QnPlIqResourceSearcher::processPacket(QnResourceList& result, QByteArray& responseData, const QHostAddress& discoveryAddress)
{

    QString smac;
    QString name;

    QList<QnNetworkResourcePtr> local_results;

    int iqpos = responseData.indexOf("IQ");


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
    
        if (net_res->getMAC().toString() == smac)
        {
            if (isNewDiscoveryAddressBetter(net_res->getHostAddress().toString(), discoveryAddress.toString(), net_res->getDiscoveryAddr().toString()))
                net_res->setDiscoveryAddr(discoveryAddress);
            return local_results; // already found;
        }
    }


    QnNetworkResourcePtr resource ( new QnPlIqResource() );

    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (!rt.isValid())
    {
        // try with default camera name
        name = QLatin1String("IQA32N");
        rt = qnResTypePool->getResourceTypeId(manufacture(), name);

        if (!rt.isValid())
            return local_results;
    }

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setMAC(smac);

    local_results.push_back(resource);

    return local_results;
}
