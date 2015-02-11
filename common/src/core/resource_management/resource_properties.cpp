#include "resource_properties.h"
#include "api/app_server_connection.h"
#include "nx_ec/dummy_handler.h"

void QnResourcePropertyDictionary::saveParams(const QnUuid& resourceId)
{
    ec2::ApiResourceParamWithRefDataList params;
    {
        SCOPED_MUTEX_LOCK( lock, &m_mutex);
        auto itr = m_modifiedItems.find(resourceId);
        if (itr == m_modifiedItems.end())
            return;
        QnResourcePropertyList& properties = itr.value();
        for (auto itrParams = properties.begin(); itrParams != properties.end(); ++itrParams)
            params.push_back(ec2::ApiResourceParamWithRefData(resourceId, itrParams.key(), itrParams.value()));
        m_modifiedItems.erase(itr);
    }

    if( params.empty() )
        return;

    ec2::ApiResourceParamWithRefDataList outData;
    ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
    ec2::ErrorCode rez = conn->getResourceManager()->saveSync(params, &outData);

    if (rez != ec2::ErrorCode::ok) 
    {
        qCritical() << Q_FUNC_INFO << ": can't save resource params to Server. Resource physicalId: "
            << resourceId << ". Description: " << ec2::toString(rez);
        addToUnsavedParams(params);
    }
}

void QnResourcePropertyDictionary::fromModifiedDataToSavedData(const QnUuid& resourceId, ec2::ApiResourceParamWithRefDataList& outData)
{
    auto itr = m_modifiedItems.find(resourceId);
    if (itr != m_modifiedItems.end()) {
        QnResourcePropertyList& properties = itr.value();
        for (auto itrParams = properties.begin(); itrParams != properties.end(); ++itrParams)
            outData.push_back(ec2::ApiResourceParamWithRefData(resourceId, itrParams.key(), itrParams.value()));
        m_modifiedItems.erase(itr);
    }
}

int QnResourcePropertyDictionary::saveData(const ec2::ApiResourceParamWithRefDataList&& data)
{
    if (data.empty())
        return -1; // nothing to save
    ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
    SCOPED_MUTEX_LOCK( lock, &m_requestMutex);
    //TODO #ak m_requestInProgress is redundant here, data can be saved to 
        //functor to use instead of \a QnResourcePropertyDictionary::onRequestDone
    int requestId = conn->getResourceManager()->save(data, this, &QnResourcePropertyDictionary::onRequestDone);
    m_requestInProgress.insert(requestId, std::move(data));
    return requestId;
}

int QnResourcePropertyDictionary::saveParamsAsync(const QnUuid& resourceId)
{
    ec2::ApiResourceParamWithRefDataList data;
    {
        SCOPED_MUTEX_LOCK( lock, &m_mutex);
        //TODO #vasilenko is it correct to mark property as saved before it has been actually saved to ec?
        fromModifiedDataToSavedData(resourceId, data);
    }
    return saveData(std::move(data));
}

int QnResourcePropertyDictionary::saveParamsAsync(const QList<QnUuid>& idList)
{
    ec2::ApiResourceParamWithRefDataList data;
    {
        SCOPED_MUTEX_LOCK( lock, &m_mutex);
        //TODO #vasilenko is it correct to mark property as saved before it has been actually saved to ec?
        for(const QnUuid& resourceId: idList) 
            fromModifiedDataToSavedData(resourceId, data);
    }
    return saveData(std::move(data));
}

void QnResourcePropertyDictionary::onRequestDone( int reqID, ec2::ErrorCode errorCode )
{
    ec2::ApiResourceParamWithRefDataList unsavedData;
    {
        SCOPED_MUTEX_LOCK( lock, &m_requestMutex);
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

void QnResourcePropertyDictionary::addToUnsavedParams(const ec2::ApiResourceParamWithRefDataList& params)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    for(const ec2::ApiResourceParamWithRefData& param: params) 
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
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    auto itr = m_items.find(resourceId);
    return itr != m_items.end() ? itr.value().value(key) : QString();
}

void QnResourcePropertyDictionary::clear()
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    m_items.clear();
    m_modifiedItems.clear();
    m_requestInProgress.clear();
}

void QnResourcePropertyDictionary::clear(const QVector<QnUuid>& idList)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    for(const QnUuid& id: idList) {
        m_items.remove(id);
        m_modifiedItems.remove(id);
    }
    //removing from m_requestInProgress
    cancelOngoingRequest(
        [&idList]( const ec2::ApiResourceParamWithRefData& param ) -> bool {
            return idList.contains( param.resourceId );
        } );
}

bool QnResourcePropertyDictionary::setValue(const QnUuid& resourceId, const QString& key, const QString& value, bool markDirty, bool replaceIfExists)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
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
        //if parameter marked as modified, removing mark. I.e. parameter value has been reset to already saved value
        QnResourcePropertyList& modifiedProperties = m_modifiedItems[resourceId];
        auto propertyIter = modifiedProperties.find( key );
        if( propertyIter != modifiedProperties.end() )
            modifiedProperties.erase( propertyIter );
    }
    return true;
}

bool QnResourcePropertyDictionary::hasProperty(const QnUuid& resourceId, const QString& key) const
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
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

bool QnResourcePropertyDictionary::removeProperty(const QnUuid& resourceId, const QString& key)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    if( !removePropertyFromDictionary(&m_items, resourceId, key) &&
        !removePropertyFromDictionary(&m_modifiedItems, resourceId, key) )
    {
        return false;
    }

    //removing from m_requestInProgress
    cancelOngoingRequest(
        [&resourceId, &key]( const ec2::ApiResourceParamWithRefData& param ) -> bool {
            return param.resourceId == resourceId && param.name == key;
        } );
    return true;
}

ec2::ApiResourceParamDataList QnResourcePropertyDictionary::allProperties(const QnUuid& resourceId) const
{
    ec2::ApiResourceParamDataList result;

    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    auto itr = m_items.find(resourceId);
    if (itr == m_items.end())
        return result;
    const QnResourcePropertyList& properties = itr.value();
    for (auto itr = properties.begin(); itr != properties.end(); ++itr)
        result.push_back(ec2::ApiResourceParamData(itr.key(), itr.value()));

    return result;
}

QHash<QnUuid, QSet<QString> > QnResourcePropertyDictionary::allPropertyNamesByResource() const {
    QHash<QnUuid, QSet<QString> > result;

    SCOPED_MUTEX_LOCK( lock, &m_mutex);
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
