#pragma once

#include <core/resource/resource.h>

class QnWebPageResource: public QnResource
{
    Q_OBJECT
    using base_type = QnResource;

public:
    QnWebPageResource();
    QnWebPageResource(const QUrl& url);

    virtual ~QnWebPageResource();

    virtual QString getUniqueId() const override;

    virtual void setUrl(const QString& url) override;

    static QString nameForUrl(const QUrl& url);
};

Q_DECLARE_METATYPE(QnWebPageResourcePtr)
Q_DECLARE_METATYPE(QnWebPageResourceList)