#ifndef _CLIENT_MEDIA_RESOURCE_H
#define _CLIENT_MEDIA_RESOURCE_H

#include "../media_resource.h"

class QnClientMediaResource: virtual public QnMediaResource
{
public:
    QnClientMediaResource();
    virtual ~QnClientMediaResource();

    virtual int getStreamDataProvidersMaxAmount() const { return INT_MAX; }
    virtual QnStreamQuality getBestQualityForSuchOnScreenSize(QSize size) const { return QnQualityPreSeted; }

    virtual QnResourcePtr updateResource() { return QnResourcePtr(this); }
    virtual bool getBasicInfo() { return true; }
    virtual void beforeUse() {}

    virtual QString manufacture() const { return m_manufacture; }
protected:
    virtual QnAbstractMediaStreamDataProvider* createMediaProvider();
private:
    QString m_manufacture;
};

#endif
