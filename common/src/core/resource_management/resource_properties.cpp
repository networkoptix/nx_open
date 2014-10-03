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
    ec2::ErrorCode rez = conn->getResourceManager()->saveSync(resourceId, params, true, &outData);

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
    int requestId = conn->getResourceManager()->save(resourceId, data.params, true, this, &QnResourcePropertyDictionary::onRequestDone);
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
