#include "axis_resource_searcher.h"
#include "core/resource/network_resource.h"
#include "axis_resource.h"



QnPlAxisResourceSearcher::QnPlAxisResourceSearcher()
{

}

QnPlAxisResourceSearcher& QnPlAxisResourceSearcher::instance()
{
    static QnPlAxisResourceSearcher inst;
    return inst;
}


QnResourcePtr QnPlAxisResourceSearcher::createResource(const QnId& resourceTypeId, const QnResourceParameters& parameters)
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
        qDebug() << "Manufature " << resourceType->getManufacture() << " != " << manufacture();

        return result;
    }

    result = QnNetworkResourcePtr( new QnPlAxisResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "RTID" << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;

}

QString QnPlAxisResourceSearcher::manufacture() const
{
    return QnPlAxisResource::MANUFACTURE;
}


QnResourcePtr QnPlAxisResourceSearcher::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr(0);
}

QnNetworkResourcePtr QnPlAxisResourceSearcher::processPacket(QnResourceList& result, QByteArray& responseData)
{

    QString smac;
    QString name;

    int iqpos = responseData.indexOf("AXIS");


    if (iqpos<0)
        return QnNetworkResourcePtr(0);

    int macpos = responseData.indexOf('-', iqpos);
    if (macpos < 0)
        return QnNetworkResourcePtr(0);

    for (int i = iqpos; i < macpos; i++)
    {
        name += QLatin1Char(responseData[i]);
    }

    name.replace(QString(" "), QString()); // remove spaces
    name.replace(QString("\t"), QString()); // remove tabs

    if (macpos+12 > responseData.size())
        return QnNetworkResourcePtr(0);


    macpos++; // -

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


    QnNetworkResourcePtr resource ( new QnPlAxisResource() );

    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (!rt.isValid())
        return QnNetworkResourcePtr(0);

    resource->setName(name);
    resource->setMAC(smac);

    return resource;


}
