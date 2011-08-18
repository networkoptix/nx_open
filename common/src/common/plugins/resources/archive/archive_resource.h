#ifndef __ARCHIVE_RESOURCE_H
#define __ARCHIVE_RESOURCE_H

#include "resource/media_resource.h"

class QnArchiveResource : virtual public QnMediaResource, virtual public QnURLResource
{
public:
    QnArchiveResource();
    QnArchiveResource(const QString& url);

    virtual int getStreamDataProvidersMaxAmount() const { return INT_MAX; }
    virtual QnStreamQuality getBestQualityForSuchOnScreenSize(QSize size) const { return QnQualityPreSeted; }

    virtual QnResourcePtr updateResource() { return QnResourcePtr(this); }
    virtual void beforeUse() {}

    virtual QString manufacture() const { return "Archive"; }

protected:
    virtual QnAbstractMediaStreamDataProvider* createMediaProvider();
};

#endif
