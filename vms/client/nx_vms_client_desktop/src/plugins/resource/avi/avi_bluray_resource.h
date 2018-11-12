#pragma once

#include <core/resource/avi/avi_resource.h>

class QnAviBlurayResource: public QnAviResource
{
    Q_OBJECT;

public:
    QnAviBlurayResource(const QString& file);
    virtual ~QnAviBlurayResource();

    virtual QnAviArchiveDelegate* createArchiveDelegate() const override;
    static bool isAcceptedUrl(const QString& url);
};
