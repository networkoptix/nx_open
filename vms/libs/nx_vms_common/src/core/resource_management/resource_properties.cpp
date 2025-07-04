// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_properties.h"

#include <nx/utils/qt_helpers.h>
#include <nx/vms/common/system_context.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_resource_manager.h>
#include <nx/utils/log/log.h>

QnResourcePropertyDictionary::QnResourcePropertyDictionary(
    nx::vms::common::SystemContext* context,
    QObject* parent)
    :
    QObject(parent),
    nx::vms::common::SystemContextAware(context)
{
}

bool QnResourcePropertyDictionary::saveParams(const nx::Uuid& resourceId)
{
    nx::vms::api::ResourceParamWithRefDataList params;
    {
        NX_WRITE_LOCKER lock(&m_readWriteLock);
        auto itr = m_modifiedItems.find(resourceId);
        if (itr == m_modifiedItems.end())
            return true;
        QnResourcePropertyList& properties = itr->second;
        for (auto itrParams = properties.begin(); itrParams != properties.end(); ++itrParams)
            params.emplace_back(resourceId, itrParams->first, itrParams->second);
        m_modifiedItems.erase(itr);
    }

    if( params.empty() )
        return true;

    ec2::AbstractECConnectionPtr conn = messageBusConnection();
    if (!conn)
    {
        NX_WARNING(this, "%1: No connection", __func__);
        return false;
    }

    ec2::ErrorCode rez = conn->getResourceManager(Qn::kSystemAccess)->saveSync(params);
    if (rez != ec2::ErrorCode::ok)
    {
        NX_WARNING(this, "Can't save resource params. Resource: %1. Error: %2",
            resourceId, ec2::toString(rez));
        return false;
    }

    return true;
}

void QnResourcePropertyDictionary::fromModifiedDataToSavedData(
    const nx::Uuid& resourceId,
    nx::vms::api::ResourceParamWithRefDataList& outData)
{
    auto itr = m_modifiedItems.find(resourceId);
    if (itr != m_modifiedItems.end())
    {
        QnResourcePropertyList& properties = itr->second;
        for (auto itrParams = properties.begin(); itrParams != properties.end(); ++itrParams)
            outData.emplace_back(resourceId, itrParams->first, itrParams->second);
        m_modifiedItems.erase(itr);
    }
}

int QnResourcePropertyDictionary::saveData(const nx::vms::api::ResourceParamWithRefDataList&& data)
{
    if (data.empty())
        return -1; // nothing to save
    ec2::AbstractECConnectionPtr conn = messageBusConnection();
    if (!conn)
        return -1; // not connected to ec2
    int requestId = conn->getResourceManager(Qn::kSystemAccess)->save(
        data,
        [this](int requestId, ec2::ErrorCode errorCode)
        {
            onRequestDone(requestId, errorCode);
        },
        this);
    return requestId;
}

int QnResourcePropertyDictionary::saveParamsAsync(const nx::Uuid& resourceId)
{
    nx::vms::api::ResourceParamWithRefDataList data;
    {
        NX_WRITE_LOCKER lock( &m_readWriteLock );
        //TODO #rvasilenko is it correct to mark property as saved before it has been actually saved to ec?
        fromModifiedDataToSavedData(resourceId, data);
    }
    return saveData(std::move(data));
}

int QnResourcePropertyDictionary::saveParamsAsync(const QList<nx::Uuid>& idList)
{
    nx::vms::api::ResourceParamWithRefDataList data;
    {
        NX_WRITE_LOCKER lock(&m_readWriteLock);
        //TODO #rvasilenko is it correct to mark property as saved before it has been actually saved to ec?
        for(const nx::Uuid& resourceId: idList)
            fromModifiedDataToSavedData(resourceId, data);
    }
    return saveData(std::move(data));
}

void QnResourcePropertyDictionary::onRequestDone( int reqID, ec2::ErrorCode errorCode )
{
    emit asyncSaveDone(reqID, errorCode);
}

QString QnResourcePropertyDictionary::value(const nx::Uuid& resourceId, const QString& key) const
{
    NX_READ_LOCKER lock(&m_readWriteLock);
    auto resToPropsIt = m_items.find(resourceId);
    if (resToPropsIt == m_items.end())
        return QString();

    auto propIt = resToPropsIt->second.find(key);
    return propIt == resToPropsIt->second.cend() ? QString() : propIt->second;
}

void QnResourcePropertyDictionary::clear()
{
    NX_WRITE_LOCKER lock( &m_readWriteLock );
    m_items.clear();
    m_modifiedItems.clear();
}

void QnResourcePropertyDictionary::clear(const QVector<nx::Uuid>& idList)
{
    NX_WRITE_LOCKER lock(&m_readWriteLock);
    for (const nx::Uuid& id: idList)
    {
        m_items.erase(id);
        m_modifiedItems.erase(id);
    }
}

void QnResourcePropertyDictionary::markAllParamsDirty(
    const nx::Uuid& resourceId,
    nx::utils::MoveOnlyFunc<bool(const QString& paramName, const QString& paramValue)> filter)
{
    NX_WRITE_LOCKER lock(&m_readWriteLock);
    auto itr = m_items.find(resourceId);
    if (itr == m_items.end())
        return;
    const QnResourcePropertyList& properties = itr->second;
    QnResourcePropertyList& modifiedProperties = m_modifiedItems[resourceId];
    for (auto itrProperty = properties.begin(); itrProperty != properties.end(); ++itrProperty)
    {
        if (!modifiedProperties.contains(itrProperty->first))
        {
            if (filter && !filter(itrProperty->first, itrProperty->second))
                continue;
            modifiedProperties[itrProperty->first] = itrProperty->second;
        }
    }
}

bool QnResourcePropertyDictionary::setValue(const nx::Uuid& resourceId, const QString& key,
    const QString& value, bool markDirty)
{
    NX_WRITE_LOCKER lock(&m_readWriteLock);
    auto itr = m_items.find(resourceId);
    if (itr == m_items.end())
        itr = m_items.emplace(resourceId, QnResourcePropertyList{}).first;

    QnResourcePropertyList& properties = itr->second;
    auto itrValue = properties.find(key);
    if (itrValue == properties.end())
        properties.emplace(key, value);
    else if (itrValue->second != value)
        itrValue->second = value;
    else
        return false; // nothing to change
    if (markDirty)
    {
        m_modifiedItems[resourceId][key] = value;
    }
    else
    {
        // If parameter marked as modified, removing mark,
        // i.e. parameter value has been reset to already saved value.
        QnResourcePropertyList& modifiedProperties = m_modifiedItems[resourceId];
        auto propertyIter = modifiedProperties.find( key );
        if( propertyIter != modifiedProperties.end() )
            modifiedProperties.erase( propertyIter );
    }

    lock.unlock();
    emit propertyChanged(resourceId, key);
    return true;
}

bool QnResourcePropertyDictionary::hasProperty(const nx::Uuid& resourceId, const QString& key) const
{
    NX_READ_LOCKER lock(&m_readWriteLock);
    auto itr = m_items.find(resourceId);
    return itr != m_items.end() && itr->second.contains(key);
}

bool QnResourcePropertyDictionary::hasProperty(const QString& key, const QString& value) const
{
    NX_READ_LOCKER lock(&m_readWriteLock);
    for (const auto& p: m_items)
    {
        auto& resProps = p.second;
        auto itr = resProps.find(key);
        if (itr != resProps.end() && itr->second == value)
            return true;
    }
    return false;
}

nx::vms::api::ResourceParamWithRefDataList QnResourcePropertyDictionary::allProperties() const
{
    using namespace nx::vms::api;
    ResourceParamWithRefDataList result;

    NX_READ_LOCKER lock(&m_readWriteLock);
    for (auto resourceItr = m_items.cbegin(); resourceItr != m_items.cend(); ++resourceItr)
    {
        const auto& resProperties = resourceItr->second;
        for (auto itr = resProperties.begin(); itr != resProperties.end(); ++itr)
        {
            result.push_back(ResourceParamWithRefData(
                resourceItr->first, itr->first, itr->second));
        }
    }
    return result;
}

namespace
{
bool removePropertyFromDictionary(
    std::unordered_map<nx::Uuid, std::unordered_map<QString, QString>>* dict,
    const nx::Uuid& resourceId,
    const QString& key )
{
    auto resIter = dict->find(resourceId);
    if(resIter == dict->cend())
        return false;

    auto propertyIter = resIter->second.find(key);
    if(propertyIter == resIter->second.cend())
        return false;

    resIter->second.erase(propertyIter);
    return true;
}
}

bool QnResourcePropertyDictionary::on_resourceParamRemoved(
    const nx::Uuid& resourceId,
    const QString& key)
{
    NX_WRITE_LOCKER lock(&m_readWriteLock);

    if (!removePropertyFromDictionary(&m_items, resourceId, key) &&
        !removePropertyFromDictionary(&m_modifiedItems, resourceId, key))
    {
        return false;
    }

    lock.unlock();
    emit propertyRemoved(resourceId, key);
    return true;
}

nx::vms::api::ResourceParamDataList QnResourcePropertyDictionary::allProperties(
    const nx::Uuid& resourceId) const
{
    nx::vms::api::ResourceParamDataList result;

    NX_READ_LOCKER lock(&m_readWriteLock);
    auto itr = m_items.find(resourceId);
    if (itr == m_items.end())
        return result;
    const QnResourcePropertyList& properties = itr->second;
    for (auto itr = properties.begin(); itr != properties.end(); ++itr)
        result.emplace_back(itr->first, itr->second);

    return result;
}

std::unordered_map<QString, QString> QnResourcePropertyDictionary::modifiedProperties(
    const nx::Uuid& resourceId) const
{
    NX_READ_LOCKER lock(&m_readWriteLock);

    auto resToPropsIt = m_modifiedItems.find(resourceId);
    return resToPropsIt == m_modifiedItems.cend()
        ? std::unordered_map<QString, QString>{}
        : resToPropsIt->second;
}

QHash<nx::Uuid, QSet<QString>> QnResourcePropertyDictionary::allPropertyNamesByResource() const
{
    QHash<nx::Uuid, QSet<QString>> result;

    NX_READ_LOCKER lock(&m_readWriteLock);
    for (auto iter = m_items.cbegin(); iter != m_items.cend(); ++iter) {
        nx::Uuid id = iter->first;
        QSet<QString> names;
        for (const auto& p: iter->second)
            names.insert(p.first);
        result.emplace(id, std::move(names));
    }
    return result;
}
