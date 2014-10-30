#ifdef ENABLE_AXIS

#include "axis_resource_searcher.h"

#include <utils/common/log.h>

#include <core/resource/camera_resource.h>

#include "axis_resource.h"

extern QString getValueFromString(const QString& line);

QnPlAxisResourceSearcher::QnPlAxisResourceSearcher()
{
}

QnResourcePtr QnPlAxisResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
{
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        NX_LOG(lit("No resource type for ID %1").arg(resourceTypeId.toString()), cl_logDEBUG1);

        return result;
    }

    if (resourceType->getManufacture() != manufacture())
    {
        return result;
    }

    result = QnVirtualCameraResourcePtr( new QnPlAxisResource() );
    result->setTypeId(resourceTypeId);

    NX_LOG(lit("Create Axis camera resource. TypeID %1.").arg(resourceTypeId.toString()), cl_logDEBUG1);


    return result;

}

QString QnPlAxisResourceSearcher::manufacture() const
{
    return QnPlAxisResource::MANUFACTURE;
}


QList<QnResourcePtr> QnPlAxisResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool isSearchAction)
{
    if( !url.scheme().isEmpty() && isSearchAction )
	
        return QList<QnResourcePtr>();  //searching if only host is present, not specific protocol
		
    QString host = url.host();
    int port = url.port();
    if (host.isEmpty())
        host = url.toString(); // in case if url just host address without protocol and port

    int timeout = 4000; // TODO: #Elric we should probably increase this one. In some cases 4 secs is not enough.


    if (port < 0)
        port = nx_http::DEFAULT_HTTP_PORT;

    CLHttpStatus status;
    QString response = QString(QLatin1String(downloadFile(status, QLatin1String("axis-cgi/param.cgi?action=list&group=Network"), host, port, timeout, auth)));

    if (response.length()==0)
        return QList<QnResourcePtr>();

    QStringList lines = response.split(QLatin1String("\n"), QString::SkipEmptyParts);


    QString name;
    QString mac;

    for(QString line: lines)
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


    QnUuid typeId = qnResTypePool->getLikeResourceTypeId(manufacture(), name);
    if (typeId.isNull())
        return QList<QnResourcePtr>();

    QnPlAxisResourcePtr resource(new QnPlAxisResource());
    resource->setTypeId(typeId);
    resource->setName(name);
    resource->setModel(name);
    resource->setMAC(QnMacAddress(mac));
    QUrl finalUrl(url);
    finalUrl.setScheme(QLatin1String("http"));
    finalUrl.setPort(port);
    resource->setUrl(finalUrl.toString());
    resource->setAuth(auth);

    //resource->setDiscoveryAddr(iface.address);
    QList<QnResourcePtr> result;
    result << resource;

    if (!isSearchAction)
        addMultichannelResources(result);

    return result;
}

QList<QnNetworkResourcePtr> QnPlAxisResourceSearcher::processPacket(
    QnResourceList& result,
    const QByteArray& responseData,
    const QHostAddress& discoveryAddress,
    const QHostAddress& /*foundHostAddress*/ )
{

    QString smac;
    QString name;

    QList<QnNetworkResourcePtr> local_results;

    int iqpos = responseData.indexOf("AXIS");


    if (iqpos<0)
        return local_results;

    QByteArray prefix2("axis-00");
    int macpos = responseData.indexOf("- 00", iqpos);
    if (macpos < 0) {
        macpos = responseData.indexOf("- AC", iqpos);
        if (macpos < 0)
            return local_results;
        prefix2 = QByteArray("axis-ac");
    }
    macpos += 2;

    for (int i = iqpos; i < macpos; i++)
    {
        name += QLatin1Char(responseData[i]);
    }

    int macpos2 = responseData.indexOf(prefix2, macpos);
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

    for(const QnResourcePtr& res: result)
    {
        QnNetworkResourcePtr net_res = res.dynamicCast<QnNetworkResource>();
    
        if (net_res->getMAC().toString() == smac) {
            return local_results; // already found;
        }
    }


    QnPlAxisResourcePtr resource ( new QnPlAxisResource() );

    QnUuid rt = qnResTypePool->getLikeResourceTypeId(manufacture(), name);
    if (rt.isNull())
        return local_results;

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setModel(name);
    resource->setMAC(QnMacAddress(smac));


    local_results.push_back(resource);


    addMultichannelResources(local_results);
    
    return local_results;
}
template <class T>
void QnPlAxisResourceSearcher::addMultichannelResources(QList<T>& result)
{
    QnPlAxisResourcePtr firstResource = result.first().template dynamicCast<QnPlAxisResource>();

    int channels = 1;
    if (firstResource->hasParam(QLatin1String("channelsAmount")))
    {
        QString val = firstResource->getProperty(QLatin1String("channelsAmount"));
        channels = val.toUInt();
    }
    if (channels > 1)
    {
        QString physicalId = firstResource->getPhysicalId();
        firstResource->setGroupName(physicalId);
        firstResource->setGroupId(physicalId);

        firstResource->setPhysicalId(physicalId + QLatin1String("_channel_") + QString::number(1));

        for (int i = 2; i <= channels; ++i)
        {
            QnPlAxisResourcePtr resource ( new QnPlAxisResource() );

            QnUuid rt = qnResTypePool->getLikeResourceTypeId(manufacture(), firstResource->getName());
            if (rt.isNull())
                return;

            resource->setTypeId(rt);
            resource->setName(firstResource->getName());
            resource->setModel(firstResource->getName());
            resource->setMAC(firstResource->getMAC());
            resource->setGroupName(physicalId);
            resource->setGroupId(physicalId);
            resource->setAuth(firstResource->getAuth());
            resource->setUrl(firstResource->getUrl());

            resource->setPhysicalId(resource->getPhysicalId() + QLatin1String("_channel_") + QString::number(i));

            result.push_back(resource);

        }
    }
}

#endif // #ifdef ENABLE_AXIS
