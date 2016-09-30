#include "abstract_resource_access_provider.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/user_resource.h>

QnAbstractResourceAccessProvider::QnAbstractResourceAccessProvider(QObject* parent):
    base_type(parent)
{
}

QnAbstractResourceAccessProvider::~QnAbstractResourceAccessProvider()
{
}

QList<QnResourceAccessSubject> QnAbstractResourceAccessProvider::allSubjects()
{
    QList<QnResourceAccessSubject> result;
    for (const auto& user : qnResPool->getResources<QnUserResource>())
        result << user;
    for (const auto& role : qnUserRolesManager->userRoles())
        result << role;
    return result;
}
