#ifndef QN_AVI_BLURAY_RESOURCE_H
#define QN_AVI_BLURAY_RESOURCE_H

#ifdef ENABLE_ARCHIVE

#include "avi_resource.h"

class QnAviBlurayResource : public QnAviResource
{
    Q_OBJECT;

public:
    QnAviBlurayResource(const QString& file);
    virtual ~QnAviBlurayResource();

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role);
    static bool isAcceptedUrl(const QString& url);

protected:

};

#endif // ENABLE_ARCHIVE

#endif // QN_AVI_BLURAY_RESOURCE_H
