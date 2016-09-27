#include "permissions_resource_access_provider.h"

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
    return false;
}
