#include "resource_properties.h"
#include "api/app_server_connection.h"
#include "nx_ec/dummy_handler.h"
#include <common/common_module.h>

QnResourcePropertyDictionary::QnResourcePropertyDictionary(QObject *parent):
    QObject(parent),
    QnCommonModuleAware(parent)
{
}

bool QnResourcePropertyDictionary::saveParams(const QnUuid& resourceId)
{
    nx::vms::api::ResourceParamWithRefDataList params;
    {
        QnMutexLocker lock( &m_mutex );
        auto itr = m_modifiedItems.find(resourceId);
        if (itr == m_modifiedItems.end())
            return true;
        QnResourcePropertyList& properties = itr.value();
        for (auto itrParams = properties.begin(); itrParams != properties.end(); ++itrParams)
            params.emplace_back(resourceId, itrParams.key(), itrParams.value());
        m_modifiedItems.erase(itr);
    }

    if( params.empty() )
        return true;

    ec2::AbstractECConnectionPtr conn = commonModule()->ec2Connection();
    // TODO: #GDM SafeMode
    ec2::ErrorCode rez = conn->makeResourceManager(Qn::kSystemAccess)->saveSync(params);

    if (rez != ec2::ErrorCode::ok)
    {
        qCritical() << Q_FUNC_INFO << ": can't save resource params to Server. Resource physicalId: "
            << resourceId << ". Description: " << ec2::toString(rez);
        addToUnsavedParams(params);
        return false;
    }
    return true;
}

void QnResourcePropertyDictionary::fromModifiedDataToSavedData(
    const QnUuid& resourceId,
    nx::vms::api::ResourceParamWithRefDataList& outData)
{
    auto itr = m_modifiedItems.find(resourceId);
    if (itr != m_modifiedItems.end())
    {
        QnResourcePropertyList& properties = itr.value();
        for (auto itrParams = properties.begin(); itrParams != properties.end(); ++itrParams)
            outData.emplace_back(resourceId, itrParams.key(), itrParams.value());
        m_modifiedItems.erase(itr);
    }
}

int QnResourcePropertyDictionary::saveData(const nx::vms::api::ResourceParamWithRefDataList&& data)
{
    if (data.empty())
        return -1; // nothing to save
    ec2::AbstractECConnectionPtr conn = commonModule()->ec2Connection();
    if (!conn)
        return -1; // not connected to ec2
    QnMutexLocker lock(&m_requestMutex);
    //TODO #ak m_requestInProgress is redundant here, data can be saved to
    //functor to use instead of \a QnResourcePropertyDictionary::onRequestDone
    // TODO: #GDM SafeMode
    int requestId = conn->makeResourceManager(Qn::kSystemAccess)->save(
        data,
        this,
        &QnResourcePropertyDictionary::onRequestDone);
    m_requestInProgress.insert(requestId, std::move(data));
    return requestId;
}

int QnResourcePropertyDictionary::saveParamsAsync(const QnUuid& resourceId)
{
    nx::vms::api::ResourceParamWithRefDataList data;
    {
        QnMutexLocker lock( &m_mutex );
        //TODO #vasilenko is it correct to mark property as saved before it has been actually saved to ec?
        fromModifiedDataToSavedData(resourceId, data);
    }
    return saveData(std::move(data));
}

int QnResourcePropertyDictionary::saveParamsAsync(const QList<QnUuid>& idList)
{
    nx::vms::api::ResourceParamWithRefDataList data;
    {
        QnMutexLocker lock( &m_mutex );
        //TODO #vasilenko is it correct to mark property as saved before it has been actually saved to ec?
        for(const QnUuid& resourceId: idList)
            fromModifiedDataToSavedData(resourceId, data);
    }
    return saveData(std::move(data));
}

void QnResourcePropertyDictionary::onRequestDone( int reqID, ec2::ErrorCode errorCode )
{
    nx::vms::api::ResourceParamWithRefDataList unsavedData;
    {
        QnMutexLocker lock( &m_requestMutex );
        auto itr = m_requestInProgress.find(reqID);
        if (itr == m_requestInProgress.end())
            return;
        if (errorCode != ec2::ErrorCode::ok)
            unsavedData = std::move(itr.value());
        m_requestInProgress.erase(itr);
    }
    if (!unsavedData.empty())
        addToUnsavedParams(unsavedData);
    emit asyncSaveDone(reqID, errorCode);
}

void QnResourcePropertyDictionary::addToUnsavedParams(
    const nx::vms::api::ResourceParamWithRefDataList& params)
{
    QnMutexLocker lock(&m_mutex);
    for (const auto& param: params)
    {
        auto itr = m_modifiedItems.find(param.resourceId);
        if (itr == m_modifiedItems.end())
            itr = m_modifiedItems.insert(param.resourceId, QnResourcePropertyList());
        QnResourcePropertyList& data = itr.value();

        if (!data.contains(param.name))
            data.insert(param.name, param.value);
    }
}

QString QnResourcePropertyDictionary::value(const QnUuid& resourceId, const QString& key) const
{
    QnMutexLocker lock( &m_mutex );
    auto itr = m_items.find(resourceId);
    return itr != m_items.end() ? itr.value().value(key) : QString();
}

void QnResourcePropertyDictionary::clear()
{
    QnMutexLocker lock( &m_mutex );
    m_items.clear();
    m_modifiedItems.clear();
    m_requestInProgress.clear();
}

void QnResourcePropertyDictionary::clear(const QVector<QnUuid>& idList)
{
    QnMutexLocker lock(&m_mutex);
    for (const QnUuid& id: idList)
    {
        m_items.remove(id);
        m_modifiedItems.remove(id);
    }
    //removing from m_requestInProgress
    cancelOngoingRequest(
        [&idList](const auto& param) -> bool
        {
            return idList.contains(param.resourceId);
        });
}

void QnResourcePropertyDictionary::markAllParamsDirty(const QnUuid& resourceId)
{
    QnMutexLocker lock(&m_mutex);
    auto itr = m_items.find(resourceId);
    if (itr == m_items.end())
        return;
    const QnResourcePropertyList& properties = itr.value();
    QnResourcePropertyList& modifiedProperties = m_modifiedItems[resourceId];
    for (auto itrProperty = properties.begin(); itrProperty != properties.end(); ++itrProperty)
    {
        if (!modifiedProperties.contains(itrProperty.key()))
            modifiedProperties[itrProperty.key()] = itrProperty.value();
    }
}

bool QnResourcePropertyDictionary::setValue(const QnUuid& resourceId, const QString& key,
    const QString& value, bool markDirty, bool replaceIfExists)
{
    QnMutexLocker lock( &m_mutex );
    auto itr = m_items.find(resourceId);
    if (itr == m_items.end())
        itr = m_items.insert(resourceId, QnResourcePropertyList());

    QnResourcePropertyList& properties = itr.value();
    auto itrValue = properties.find(key);
    if (itrValue == properties.end())
        properties.insert(key, value);
    else if (replaceIfExists && (itrValue.value() != value))
        itrValue.value() = value;
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
    return true;
}

bool QnResourcePropertyDictionary::hasProperty(const QnUuid& resourceId, const QString& key) const
{
    QnMutexLocker lock( &m_mutex );
    auto itr = m_items.find(resourceId);
    return itr != m_items.end() && itr.value().contains(key);
}

namespace
{
    bool removePropertyFromDictionary(
        QMap<QnUuid, QnResourcePropertyList>* const dict,
        const QnUuid& resourceId,
        const QString& key )
    {
        auto resIter = dict->find( resourceId );
        if( resIter == dict->cend() )
            return false;

        auto propertyIter = resIter.value().find( key );
        if( propertyIter == resIter.value().cend() )
            return false;

        resIter.value().erase( propertyIter );
        return true;
    }
}

bool QnResourcePropertyDictionary::on_resourceParamRemoved(
    const QnUuid& resourceId,
    const QString& key)
{
    QnMutexLocker lock(&m_mutex);

    if (!removePropertyFromDictionary(&m_items, resourceId, key) &&
        !removePropertyFromDictionary(&m_modifiedItems, resourceId, key))
    {
        return false;
    }

    //removing from m_requestInProgress
    cancelOngoingRequest(
        [&resourceId, &key](const auto& param) -> bool
        {
            return param.resourceId == resourceId && param.name == key;
        });
    return true;
}

nx::vms::api::ResourceParamDataList QnResourcePropertyDictionary::allProperties(
    const QnUuid& resourceId) const
{
    nx::vms::api::ResourceParamDataList result;

    QnMutexLocker lock( &m_mutex );
    auto itr = m_items.find(resourceId);
    if (itr == m_items.end())
        return result;
    const QnResourcePropertyList& properties = itr.value();
    for (auto itr = properties.begin(); itr != properties.end(); ++itr)
        result.emplace_back(itr.key(), itr.value());

    return result;
}

QHash<QnUuid, QSet<QString>> QnResourcePropertyDictionary::allPropertyNamesByResource() const
{
    QHash<QnUuid, QSet<QString>> result;

    QnMutexLocker lock( &m_mutex );
    for (auto iter = m_items.constBegin(); iter != m_items.constEnd(); ++iter) {
        QnUuid id = iter.key();
        result.insert(id, iter.value().keys().toSet());
    }
    return result;
}

template<class Pred>
void QnResourcePropertyDictionary::cancelOngoingRequest(const Pred& pred)
{
    for( auto ongoingRequestData: m_requestInProgress )
    {
        ongoingRequestData.erase(
            std::remove_if( ongoingRequestData.begin(), ongoingRequestData.end(), pred ),
            ongoingRequestData.end() );
    }
}
