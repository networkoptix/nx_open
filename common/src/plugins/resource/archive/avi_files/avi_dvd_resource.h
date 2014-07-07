#ifndef QN_AVI_DVD_RESOURCE_H
#define QN_AVI_DVD_RESOURCE_H

#ifdef ENABLE_ARCHIVE

#include "avi_resource.h"

class QnAviDvdResource : public QnAviResource
{
    Q_OBJECT;

public:
    QnAviDvdResource(const QString& file);
    virtual ~QnAviDvdResource();

    static bool isAcceptedUrl(const QString& url);
    static QString urlToFirstVTS(const QString& url);
protected:
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role);
};

typedef QnSharedResourcePointer<QnAviDvdResource> QnAviDvdResourcePtr;

#endif // ENABLE_ARCHIVE

#endif // QN_AVI_DVD_RESOURCE_H
