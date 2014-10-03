#include "resource_properties.h"
#include "api/app_server_connection.h"
#include "nx_ec/dummy_handler.h"

void QnResourcePropertyDictionary::saveParams(const QnUuid& resourceId)
{
    ec2::ApiResourceParamDataList params;
    {
        QMutexLocker lock(&m_mutex);
        auto itr = m_modifiedItems.find(resourceId);
        if (itr == m_modifiedItems.end())
            return;
        QnResourcePropertyList& properties = itr.value();
        for (auto itrParams = properties.begin(); itrParams != properties.end(); ++itrParams)
            params.push_back(ec2::ApiResourceParamData(itrParams.key(), itrParams.value()));
        m_modifiedItems.erase(itr);
    }

    ec2::ApiResourceParamListWithIdData outData;
    ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
    ec2::ErrorCode rez = conn->getResourceManager()->saveSync(resourceId, params, &outData);

    if (rez != ec2::ErrorCode::ok) 
    {
        qCritical() << Q_FUNC_INFO << ": can't save resource params to Server. Resource physicalId: "
            << resourceId << ". Description: " << ec2::toString(rez);
        QMutexLocker lock(&m_mutex);
        addToUnsavedParams(resourceId, params);
    }
}

void QnResourcePropertyDictionary::saveParamsAsync(const QnUuid& resourceId)
{
    ec2::ApiResourceParamListWithIdData data;
    data.id = resourceId;

    QMutexLocker lock(&m_mutex);
    auto itr = m_modifiedItems.find(resourceId);
    if (itr == m_modifiedItems.end())
        return;
    QnResourcePropertyList& properties = itr.value();
    for (auto itrParams = properties.begin(); itrParams != properties.end(); ++itrParams)
        data.params.push_back(ec2::ApiResourceParamData(itrParams.key(), itrParams.value()));
    m_modifiedItems.erase(itr);

    ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
    int requestId = conn->getResourceManager()->save(resourceId, data.params, this, &QnResourcePropertyDictionary::onRequestDone);
    m_requestInProgress.insert(requestId, data);
}

void QnResourcePropertyDictionary::onRequestDone( int reqID, ec2::ErrorCode errorCode )
{
    QMutexLocker lock(&m_mutex);
    auto itr = m_requestInProgress.find(reqID);
    if (itr == m_requestInProgress.end())
        return;
    ec2::ApiResourceParamListWithIdData data = itr.value();
    m_requestInProgress.erase(itr);
    if (errorCode != ec2::ErrorCode::ok) {
        addToUnsavedParams(data.id, data.params);
    }
}

void QnResourcePropertyDictionary::addToUnsavedParams(const QnUuid& resourceId, const ec2::ApiResourceParamDataList& params)
{
    auto itr = m_modifiedItems.find(resourceId);
    if (itr == m_modifiedItems.end())
        itr = m_modifiedItems.insert(resourceId, QnResourcePropertyList());
    QnResourcePropertyList& data = itr.value();

    foreach(const ec2::ApiResourceParamData& param, params) {
        if (!data.contains(param.name))
            data.insert(param.name, param.value);
    }
}

QString QnResourcePropertyDictionary::value(const QnUuid& resourceId, const QString& key) const
{
    QMutexLocker lock(&m_mutex);
    auto itr = m_items.find(resourceId);
    return itr != m_items.end() ? itr.value().value(key) : QString();
}

bool QnResourcePropertyDictionary::setValue(const QnUuid& resourceId, const QString& key, const QString& value)
{
    QMutexLocker lock(&m_mutex);
    auto itr = m_items.find(resourceId);
    if (itr == m_items.end())
        itr = m_items.insert(resourceId, QnResourcePropertyList());
    
    QnResourcePropertyList& properties = itr.value();
    auto itrValue = properties.find(key);
    if (itrValue == properties.end())
        properties.insert(key, value);
    else if (itrValue.value() != value)
        itrValue.value() = value;
    else
        return false; // nothing to change

    m_modifiedItems[resourceId][key] = value;
    return true;
}

bool QnResourcePropertyDictionary::hasProperty(const QnUuid& resourceId, const QString& key) const
{
    QMutexLocker lock(&m_mutex);
    auto itr = m_items.find(resourceId);
    return itr != m_items.end() && itr.value().contains(key);
}

ec2::ApiResourceParamDataList QnResourcePropertyDictionary::allProperties(const QnUuid& resourceId) const
{
    ec2::ApiResourceParamDataList result;

    QMutexLocker lock(&m_mutex);
    auto itr = m_items.find(resourceId);
    if (itr == m_items.end())
        return result;
    const QnResourcePropertyList& properties = itr.value();
    for (auto itr = properties.begin(); itr != properties.end(); ++itr)
        result.push_back(ec2::ApiResourceParamData(itr.key(), itr.value()));

    return result;
}
