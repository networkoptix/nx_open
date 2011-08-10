#include "archive_resource.h"
#include "avi_archive_dataprovider.h"

QnArchiveResource::QnArchiveResource(): QnMediaResource(), QnURLResource()
{

}

QnArchiveResource::QnArchiveResource(const QString& url): QnMediaResource(), QnURLResource(url)
{

}

QnAbstractMediaStreamDataProvider* QnArchiveResource::createMediaProvider()
{
    return new QnAviArchiveDataProvider(QnResourcePtr(this));
}
