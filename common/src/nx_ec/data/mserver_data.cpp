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

void ApiStorageData::fromResource(QnAbstractStorageResourcePtr resource)
{
    ApiResourceData::fromResource(resource);
    spaceLimit = resource->getSpaceLimit();
    usedForWriting = resource->isUsedForWriting();
}

void ApiStorageData::toResource(QnAbstractStorageResourcePtr resource) const
{
    ApiResourceData::toResource(resource);
    resource->setSpaceLimit(spaceLimit);
    resource->setUsedForWriting(usedForWriting);
}

QnResourceParameters ApiStorageData::toHashMap() const
{
    QnResourceParameters parameters;
    parameters["spaceLimit"] = QString::number(spaceLimit);
    parameters["usedForWriting"] = QString::number(usedForWriting);

    return parameters;
}

void ApiStorageDataList::loadFromQuery(QSqlQuery& query)
{
    QN_QUERY_TO_DATA_OBJECT(query, ApiStorageData, data, ApiStorageDataFields ApiResourceDataFields)
}

void ApiMediaServerData::fromResource(QnMediaServerResourcePtr resource)
{
    ApiResourceData::fromResource(resource);

    netAddrList = serializeNetAddrList(resource->getNetAddrList());
    apiUrl = resource->getUrl();
    reserve = resource->getReserve();
    panicMode = resource->getPanicMode();
    streamingUrl = resource->getStreamingUrl();
    version = resource->getVersion().toString();
    //authKey = resource-> getetAuthKey();

    QnAbstractStorageResourceList storageList = resource->getStorages();
    storages.resize(storageList.size());
    for (int i = 0; i < storageList.size(); ++i)
        storages[i].fromResource(storageList[i]);
}

void ApiMediaServerData::toResource(QnMediaServerResourcePtr resource, const ResourceContext& ctx) const
{
    ApiResourceData::toResource(resource);

    QList<QHostAddress> resNetAddrList;
    deserializeNetAddrList(resNetAddrList, netAddrList);

    resource->setApiUrl(apiUrl);
    resource->setNetAddrList(resNetAddrList);
    resource->setReserve(reserve);
    resource->setPanicMode((Qn::PanicMode) panicMode);
    resource->setStreamingUrl(streamingUrl);
    resource->setVersion(QnSoftwareVersion(version));
    //resource->setAuthKey(authKey);

    QnResourceTypePtr resType = ctx.resTypePool->getResourceTypeByName(QLatin1String("Storage"));
    if (!resType)
        return;

    QnAbstractStorageResourceList storagesRes;
    foreach(const ApiStorageData& storage, storages) {
        QnAbstractStorageResourcePtr storageRes = ctx.resFactory->createResource(resType->getId(), storage.toHashMap()).dynamicCast<QnAbstractStorageResource>();
        
        storage.toResource(storageRes);
        storagesRes.push_back(storageRes);
    }
    resource->setStorages(storagesRes);
}

template <class T> void ApiMediaServerDataList::toResourceList(QList<T>& outData, const ResourceContext& ctx) const
{
    outData.reserve(outData.size() + data.size());
    for(int i = 0; i < data.size(); ++i) 
    {
        QnMediaServerResourcePtr server(new QnMediaServerResource(ctx.resTypePool));
        data[i].toResource(server, ctx);
        outData << server;
    }
}
template void ApiMediaServerDataList::toResourceList<QnResourcePtr>(QList<QnResourcePtr>& outData, const ResourceContext& ctx) const;
template void ApiMediaServerDataList::toResourceList<QnMediaServerResourcePtr>(QList<QnMediaServerResourcePtr>& outData, const ResourceContext& ctx) const;


void ApiMediaServerDataList::loadFromQuery(QSqlQuery& query)
{
    QN_QUERY_TO_DATA_OBJECT(query, ApiMediaServerData, data, medisServerDataFields ApiResourceDataFields)
}

}
