#include "videowall_resource_access_provider.h"

QnVideoWallResourceAccessProvider::QnVideoWallResourceAccessProvider(QObject* parent):
    base_type(parent)
{
}

QnVideoWallResourceAccessProvider::~QnVideoWallResourceAccessProvider()
{
}

bool QnVideoWallResourceAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    return false;
}
