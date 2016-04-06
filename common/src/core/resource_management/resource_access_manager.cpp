#include "resource_access_manager.h"

#include <api/app_server_connection.h>
#include <nx_ec/managers/abstract_user_manager.h>

#include <nx_ec/data/api_access_rights_data.h>

QnResourceAccessManager::QnResourceAccessManager(QObject* parent /*= nullptr*/)
{

}

void QnResourceAccessManager::reset(const ec2::ApiAccessRightsDataList& accessRights)
{
    m_values = accessRights;
}

ec2::ApiAccessRightsDataList QnResourceAccessManager::allAccessRights() const
{
    return m_values;
}

ec2::ApiAccessRightsData QnResourceAccessManager::accessRights(const QnUuid& userId) const
{
    auto userAccessRights = std::find_if(m_values.cbegin(), m_values.cend(), [userId](const ec2::ApiAccessRightsData& data)
    {
        return data.userId == userId;
    });
    if (userAccessRights != m_values.cend())
        return *userAccessRights;

    ec2::ApiAccessRightsData result;
    result.userId = userId;
    return result;
}

void QnResourceAccessManager::setAccessRights(const ec2::ApiAccessRightsData& accessRights)
{
    auto userAccessRights = std::find_if(m_values.begin(), m_values.end(), [accessRights](const ec2::ApiAccessRightsData& data)
    {
        return data.userId == accessRights.userId;
    });

    if (userAccessRights != m_values.end())
    {
        if (userAccessRights->resourceIds == accessRights.resourceIds)
            return;
        userAccessRights->resourceIds = accessRights.resourceIds;
    }
    else
    {
        m_values.push_back(accessRights);
    }
}
