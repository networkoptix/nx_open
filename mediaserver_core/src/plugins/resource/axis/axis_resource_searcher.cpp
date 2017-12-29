#ifdef ENABLE_AXIS

#include "axis_resource.h"
#include "axis_resource_searcher.h"

#include <nx/utils/log/log.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <plugins/resource/mdns/mdns_packet.h>
#include <utils/common/credentials.h>
#include <common/static_common_module.h>

extern QString getValueFromString(const QString& line);

namespace
{
    const QString kTestCredentialsUrl = lit("axis-cgi/param.cgi?action=list&group=root.Network.Bonjour.FriendlyName");
    const int kDefaultAxisTimeout = 4000;
}

QnPlAxisResourceSearcher::QnPlAxisResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule),
    QnMdnsResourceSearcher(commonModule)
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


QList<QnResourcePtr> QnPlAxisResourceSearcher::checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool isSearchAction)
{
    if( !url.scheme().isEmpty() && isSearchAction )

        return QList<QnResourcePtr>();  //searching if only host is present, not specific protocol

    QString host = url.host();
    int port = url.port();
    if (host.isEmpty())
        host = url.toString(); // in case if url just host address without protocol and port

    int timeout = 4000; // TODO: #Elric we should probably increase this one. In some cases 4 secs is not enough.


    if (port < 0)
        port = nx::network::http::DEFAULT_HTTP_PORT;

    CLHttpStatus status;
    //QString response = QString(QLatin1String(downloadFile(status, QLatin1String("axis-cgi/param.cgi?action=list&group=Network"), host, port, timeout, auth)));
    QByteArray response1 = downloadFile(status, QLatin1String("axis-cgi/param.cgi?action=list&group=root.Network.Bonjour.FriendlyName"), host, port, timeout, auth);
    if (response1.length()==0)
        return QList<QnResourcePtr>();
    QByteArray response2 = downloadFile(status, QLatin1String("axis-cgi/param.cgi?action=list&group=root.Network.eth0.MACAddress"), host, port, timeout, auth);
    if (response2.length()==0)
        return QList<QnResourcePtr>();
    QString response = QString(QLatin1String(response1.append(response2)));
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

    QnResourceData resourceData = qnStaticCommon->dataPool()->data(manufacture(), name);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return QList<QnResourcePtr>(); // model forced by ONVIF

    QnPlAxisResourcePtr resource(new QnPlAxisResource());
    resource->setTypeId(typeId);
    resource->setName(name);
    resource->setModel(name);
    resource->setMAC(nx::network::QnMacAddress(mac));
    nx::utils::Url finalUrl(url);
    finalUrl.setScheme(QLatin1String("http"));
    finalUrl.setPort(port);
    resource->setUrl(finalUrl.toString());
    resource->setDefaultAuth(auth);

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
    const QHostAddress& foundHostAddress )
{
    Q_UNUSED(discoveryAddress)
    Q_UNUSED(foundHostAddress)

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
        const unsigned char c = responseData.at(i);
        if (c < 0x20 || c == 0x7F) // Control char.
            name += QLatin1Char('?');
        else
            name += QLatin1Char(c); //< Assume Latin-1 encoding.
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

    QnResourceData resourceData = qnStaticCommon->dataPool()->data(manufacture(), name);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return local_results; // model forced by ONVIF

    QnPlAxisResourcePtr resource ( new QnPlAxisResource() );

    QnUuid rt = qnResTypePool->getLikeResourceTypeId(manufacture(), name);
    if (rt.isNull())
        return local_results;

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setModel(name);
    resource->setMAC(nx::network::QnMacAddress(smac));

    quint16 port = nx::network::http::DEFAULT_HTTP_PORT;
    QnMdnsPacket packet;
    if (packet.fromDatagram(responseData))
    {
        auto rrsToInspect = packet.answerRRs + packet.additionalRRs;

        for (const auto& record: rrsToInspect)
        {
            if (record.recordType == QnMdnsPacket::kSrvRecordType)
            {
                QnMdnsSrvData srv;
                srv.decode(record.data);
                port = srv.port;
            }
        }
    }

    QUrl url;
    url.setScheme(lit("http"));
    url.setHost(foundHostAddress.toString());
    url.setPort(port);

    resource->setUrl(url.toString());

    auto auth = determineResourceCredentials(resource);
    resource->setDefaultAuth(auth);

    local_results.push_back(resource);

    addMultichannelResources(local_results);

    return local_results;
}

bool QnPlAxisResourceSearcher::testCredentials(
    const QUrl& url,
    const QAuthenticator& auth) const
{
    auto host = url.host();
    auto port = url.port(nx::network::http::DEFAULT_HTTP_PORT);

    CLHttpStatus status;
    auto response = downloadFile(status, kTestCredentialsUrl, host, port, kDefaultAxisTimeout, auth);

    if (status != CL_HTTP_SUCCESS)
        return false;

    return true;
}

QAuthenticator QnPlAxisResourceSearcher::determineResourceCredentials(
    const QnSecurityCamResourcePtr& resource) const
{
    if (!resource)
        return QAuthenticator();

    auto existingResource = resourcePool()->getNetResourceByPhysicalId(resource->getPhysicalId());
    if (existingResource)
        return existingResource->getAuth();

    auto resData = qnStaticCommon->dataPool()->data(resource->getVendor(), resource->getModel());
    auto possibleCredentials = resData.value<QList<nx::common::utils::Credentials>>(
        Qn::POSSIBLE_DEFAULT_CREDENTIALS_PARAM_NAME);

    for (const auto& credentials: possibleCredentials)
    {
        const auto& auth = credentials.toAuthenticator();
        if (testCredentials(resource->getUrl(), auth))
            return auth;
    }

    return QAuthenticator();
}

template <typename T>
void QnPlAxisResourceSearcher::addMultichannelResources(QList<T>& result)
{
    QnPlAxisResourcePtr firstResource = result.first().template dynamicCast<QnPlAxisResource>();

    uint channels = 1;
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

        for (uint i = 2; i <= channels; ++i)
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

            auto auth = firstResource->getAuth();
            if (!auth.isNull())
                resource->setDefaultAuth(auth);

            resource->setUrl(firstResource->getUrl());

            resource->setPhysicalId(resource->getPhysicalId() + QLatin1String("_channel_") + QString::number(i));

            result.push_back(resource);
        }
    }
}

#endif // #ifdef ENABLE_AXIS
