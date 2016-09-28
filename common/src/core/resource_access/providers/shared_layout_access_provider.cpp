#include "shared_layout_access_provider.h"

QnSharedLayoutAccessProvider::QnSharedLayoutAccessProvider(QObject* parent):
    base_type(parent)
{

}

QnSharedLayoutAccessProvider::~QnSharedLayoutAccessProvider()
{

}

bool QnSharedLayoutAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    return false;
}
