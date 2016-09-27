#include "permissions_resource_access_provider.h"

#include <core/resource_access/resource_access_manager.h>

QnPermissionsResourceAccessProvider::QnPermissionsResourceAccessProvider(QObject* parent):
    base_type(parent)
{
}

QnPermissionsResourceAccessProvider::~QnPermissionsResourceAccessProvider()
{
}

bool QnPermissionsResourceAccessProvider::hasAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    if (!resource)
        return false;

    return qnResourceAccessManager->hasGlobalPermission(subject, Qn::GlobalAdminPermission);
}
