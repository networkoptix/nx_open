#include "mserver_data.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/storage_resource.h"
#include "core/resource/resource_type.h"

//QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(ApiStorageData, ApiResourceData, ApiStorageFields);
QN_FUSION_DECLARE_FUNCTIONS(ApiStorageData, (binary))

    //QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(ApiMediaServerData, ApiResourceData, medisServerDataFields);
    QN_FUSION_DECLARE_FUNCTIONS(ApiMediaServerData, (binary))

    QN_DEFINE_API_OBJECT_LIST_DATA(ApiMediaServerData)


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

void fromResourceToApi(const QnAbstractStorageResourcePtr &resource, ApiStorageData &data)
{
    fromResourceToApi(resource, (ApiResourceData &)data);
    data.spaceLimit = resource->getSpaceLimit();
    data.usedForWriting = resource->isUsedForWriting();
}

void fromApiToResource(const ApiStorageData &data, QnAbstractStorageResourcePtr& resource)
{
    fromApiToResource((const ApiResourceData &)data, resource);
    resource->setSpaceLimit(data.spaceLimit);
    resource->setUsedForWriting(data.usedForWriting);
}

void ApiStorageList::loadFromQuery(QSqlQuery& query)
{
    QN_QUERY_TO_DATA_OBJECT(query, ApiStorageData, data, ApiStorageFields ApiResourceFields)
}

void fromResourceToApi(const QnMediaServerResourcePtr& resource, ApiMediaServerData& data)
{
    fromResourceToApi(resource, (ApiResourceData &)data);

    data.netAddrList = serializeNetAddrList(resource->getNetAddrList());
    data.apiUrl = resource->getApiUrl();
    data.flags = resource->getServerFlags();
    data.panicMode = resource->getPanicMode();
    data.streamingUrl = resource->getStreamingUrl();
    data.version = resource->getVersion().toString();
    data.maxCameras = resource->getMaxCameras();
    data.redundancy = resource->isRedundancy();
    //authKey = resource-> getetAuthKey();

    QnAbstractStorageResourceList storageList = resource->getStorages();
    data.storages.resize(storageList.size());
    for (int i = 0; i < storageList.size(); ++i)
        fromResourceToApi(storageList[i], data.storages[i]);
}

void fromApiToResource(const ApiMediaServerData &data, QnMediaServerResourcePtr& resource, const ResourceContext& ctx)
{
    fromApiToResource((const ApiResourceData &)data, resource);

    QList<QHostAddress> resNetAddrList;
    deserializeNetAddrList(resNetAddrList, data.netAddrList);

    resource->setApiUrl(data.apiUrl);
    resource->setNetAddrList(resNetAddrList);
    resource->setServerFlags(data.flags);
    resource->setPanicMode((Qn::PanicMode) data.panicMode);
    resource->setStreamingUrl(data.streamingUrl);
    resource->setVersion(QnSoftwareVersion(data.version));
    resource->setMaxCameras(data.maxCameras);
    resource->setRedundancy(data.redundancy);

    //resource->setAuthKey(authKey);

    QnResourceTypePtr resType = ctx.resTypePool->getResourceTypeByName(QLatin1String("Storage"));
    if (!resType)
        return;

    QnAbstractStorageResourceList storagesRes;
    foreach(const ApiStorageData& storage, data.storages) {
        QnAbstractStorageResourcePtr storageRes = ctx.resFactory->createResource(resType->getId(), QnResourceParams(storage.url, QString())).dynamicCast<QnAbstractStorageResource>();
        
        fromApiToResource(storage, storageRes);
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
        fromApiToResource(data[i], server, ctx);
        outData << server;
    }
}
template void ApiMediaServerList::toResourceList<QnResourcePtr>(QList<QnResourcePtr>& outData, const ResourceContext& ctx) const;
template void ApiMediaServerList::toResourceList<QnMediaServerResourcePtr>(QList<QnMediaServerResourcePtr>& outData, const ResourceContext& ctx) const;


void ApiMediaServerList::loadFromQuery(QSqlQuery& query)
{
    QN_QUERY_TO_DATA_OBJECT(query, ApiMediaServerData, data, medisServerDataFields ApiResourceFields)
}

}
