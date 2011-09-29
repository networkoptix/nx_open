#include "archive_resource.h"
#include "avi_archive_dataprovider.h"
#include "resourcecontrol/resource_pool.h"

QnArchiveResource::QnArchiveResource(): QnMediaResource(), QnURLResource()
{

}

QnArchiveResource::QnArchiveResource(const QString& url): QnMediaResource(), QnURLResource(url)
{

}

QnAbstractMediaStreamDataProvider* QnArchiveResource::createMediaProvider()
{
    QnResourcePtr res = QnResourcePool::instance().getResourceById(getId());
    QnAviArchiveDataProvider* result = new QnAviArchiveDataProvider(res);
    if (result->init())
        return result;
    else {
        delete result;
        return 0;
    }
}
