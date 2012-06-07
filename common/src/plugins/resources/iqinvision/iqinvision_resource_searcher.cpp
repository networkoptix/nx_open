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

QnNetworkResourcePtr QnPlIqResourceSearcher::createResource() const
{
    return QnVirtualCameraResourcePtr( new QnPlIqResource() );
}

QString QnPlIqResourceSearcher::manufacture() const
{
    return QnPlIqResource::MANUFACTURE;
}


QnResourcePtr QnPlIqResourceSearcher::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr(0);
}

/*QnNetworkResourcePtr QnPlIqResourceSearcher::processPacket(QnResourceList& result, QByteArray& responseData, const QHostAddress&)
{

    QString smac;
    QString name;

    int iqpos = responseData.indexOf("IQ");


    if (iqpos<0)
        return QnNetworkResourcePtr(0);

    int macpos = responseData.indexOf("00", iqpos);
    if (macpos < 0)
        return QnNetworkResourcePtr(0);

    for (int i = iqpos; i < macpos; i++)
    {
        name += QLatin1Char(responseData[i]);
    }

    name.replace(QString(" "), QString()); // remove spaces
    name.replace(QString("-"), QString()); // remove spaces
    name.replace(QString("\t"), QString()); // remove tabs

    if (macpos+12 > responseData.size())
        return QnNetworkResourcePtr(0);


    //macpos++; // -

    while(responseData.at(macpos)==' ')
        ++macpos;


    if (macpos+12 > responseData.size())
        return QnNetworkResourcePtr(0);



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
            return QnNetworkResourcePtr(0); // already found;
    }


    QnNetworkResourcePtr resource ( new QnPlIqResource() );

    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (!rt.isValid())
    {
        // try with default camera name
        name = "IQA32N";
        rt = qnResTypePool->getResourceTypeId(manufacture(), name);

        if (!rt.isValid())
            return QnNetworkResourcePtr(0);
    }

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setMAC(smac);

    return resource;


}*/
