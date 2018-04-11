#pragma once

#include <core/resource/avi/avi_resource.h>

class QnAviDvdResource : public QnAviResource
{
    Q_OBJECT;

public:
    QnAviDvdResource(const QString& file);
    virtual ~QnAviDvdResource();

    static bool isAcceptedUrl(const QString& url);
    static QString urlToFirstVTS(const QString& url);

    virtual QnAviArchiveDelegate* createArchiveDelegate() const override;
};

typedef QnSharedResourcePointer<QnAviDvdResource> QnAviDvdResourcePtr;
