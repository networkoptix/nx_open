#include "abstract_resource_access_provider.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/user_resource.h>

QnAbstractResourceAccessProvider::QnAbstractResourceAccessProvider(QObject* parent):
    base_type(parent),
    QnUpdatable()
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

QList<QnResourceAccessSubject> QnAbstractResourceAccessProvider::dependentSubjects(
    const QnResourceAccessSubject& subject)
{
    if (!subject.isValid() || subject.user())
        return QList<QnResourceAccessSubject>();

    QList<QnResourceAccessSubject> result;
    auto roleId = subject.id();
    for (const auto& user : qnResPool->getResources<QnUserResource>())
    {
        if (QnResourceAccessSubject(user).effectiveId() == roleId)
            result << user;
    }
    return result;
}
