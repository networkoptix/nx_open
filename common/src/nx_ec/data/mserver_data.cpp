#include "mserver_data.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/storage_resource.h"
#include "core/resource/resource_type.h"

namespace ec2
{

static QHostAddress stringToAddr(const QString& hostStr)
{
    return QHostAddress(hostStr);
}


static void deserializeNetAddrList(QList<QHostAddress>& netAddrList, const QString& netAddrListString)
{
    QStringList addListStrings = netAddrListString.split(QLatin1Char(';'));
    std::transform(addListStrings.begin(), addListStrings.end(), std::back_inserter(netAddrList), stringToAddr);
}

static QString serializeNetAddrList(const QList<QHostAddress>& netAddrList)
{
    QStringList addListStrings;
    std::transform(netAddrList.begin(), netAddrList.end(), std::back_inserter(addListStrings), std::mem_fun_ref(&QHostAddress::toString));
    return addListStrings.join(QLatin1String(";"));
}

void ApiStorage::fromResource(QnAbstractStorageResourcePtr resource)
{
    ApiResource::fromResource(resource);
    spaceLimit = resource->getSpaceLimit();
    usedForWriting = resource->isUsedForWriting();
}

void ApiStorage::toResource(QnAbstractStorageResourcePtr resource) const
{
    ApiResource::toResource(resource);
    resource->setSpaceLimit(spaceLimit);
    resource->setUsedForWriting(usedForWriting);
}

void ApiStorageList::loadFromQuery(QSqlQuery& query)
{
    QN_QUERY_TO_DATA_OBJECT(query, ApiStorage, data, ApiStorageFields ApiResourceFields)
}

void ApiMediaServer::fromResource(QnMediaServerResourcePtr resource)
{
    ApiResource::fromResource(resource);

    netAddrList = serializeNetAddrList(resource->getNetAddrList());
    apiUrl = resource->getApiUrl();
    flags = resource->getServerFlags();
    panicMode = resource->getPanicMode();
    streamingUrl = resource->getStreamingUrl();
    version = resource->getVersion().toString();
    systemInfo = resource->getSystemInfo().toString();
    maxCameras = resource->getMaxCameras();
    redundancy = resource->isRedundancy();
    //authKey = resource-> getetAuthKey();

    QnAbstractStorageResourceList storageList = resource->getStorages();
    storages.resize(storageList.size());
    for (int i = 0; i < storageList.size(); ++i)
        storages[i].fromResource(storageList[i]);
}

void ApiMediaServer::toResource(QnMediaServerResourcePtr resource, const ResourceContext& ctx) const
{
    ApiResource::toResource(resource);

    QList<QHostAddress> resNetAddrList;
    deserializeNetAddrList(resNetAddrList, netAddrList);

    resource->setApiUrl(apiUrl);
    resource->setNetAddrList(resNetAddrList);
    resource->setServerFlags(flags);
    resource->setPanicMode((Qn::PanicMode) panicMode);
    resource->setStreamingUrl(streamingUrl);
    resource->setVersion(QnSoftwareVersion(version));
    resource->setSystemInfo(QnSystemInformation(systemInfo));
    resource->setMaxCameras(maxCameras);
    resource->setRedundancy(redundancy);

    //resource->setAuthKey(authKey);

    QnResourceTypePtr resType = ctx.resTypePool->getResourceTypeByName(QLatin1String("Storage"));
    if (!resType)
        return;

    QnAbstractStorageResourceList storagesRes;
    foreach(const ApiStorage& storage, storages) {
        QnAbstractStorageResourcePtr storageRes = ctx.resFactory->createResource(resType->getId(), QnResourceParams(storage.url, QString())).dynamicCast<QnAbstractStorageResource>();
        
        storage.toResource(storageRes);
        storagesRes.push_back(storageRes);
    }
    resource->setStorages(storagesRes);
}

template <class T> void ApiMediaServerList::toResourceList(QList<T>& outData, const ResourceContext& ctx) const
{
    outData.reserve(outData.size() + data.size());
    for(int i = 0; i < data.size(); ++i) 
    {
        QnMediaServerResourcePtr server(new QnMediaServerResource(ctx.resTypePool));
        data[i].toResource(server, ctx);
        outData << server;
    }
}
template void ApiMediaServerList::toResourceList<QnResourcePtr>(QList<QnResourcePtr>& outData, const ResourceContext& ctx) const;
template void ApiMediaServerList::toResourceList<QnMediaServerResourcePtr>(QList<QnMediaServerResourcePtr>& outData, const ResourceContext& ctx) const;


void ApiMediaServerList::loadFromQuery(QSqlQuery& query)
{
    QN_QUERY_TO_DATA_OBJECT(query, ApiMediaServer, data, medisServerDataFields ApiResourceFields)
}

}
