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
    return QnPlIsdResource::MANUFACTURE;
}


QnResourcePtr QnPlISDResourceSearcher::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr(0);
}

QnNetworkResourcePtr QnPlISDResourceSearcher::processPacket(QnResourceList& result, QByteArray& responseData)
{

    QString smac;
    QString name;

    int iqpos = responseData.indexOf("ISD");


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


    QnNetworkResourcePtr resource ( new QnPlIsdResource() );

    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (!rt.isValid())
    {
        return QnNetworkResourcePtr(0);
    }

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setMAC(smac);

    return resource;


}
