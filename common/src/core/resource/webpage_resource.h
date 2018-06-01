#pragma once

#include <core/resource/resource.h>

class QnWebPageResource : public QnResource
{
    Q_OBJECT
    using base_type = QnResource;

public:
    QnWebPageResource(QnCommonModule* commonModule = nullptr);
    QnWebPageResource(const QUrl& url, QnCommonModule* commonModule = nullptr);

    virtual ~QnWebPageResource();

    virtual void setUrl(const QString& url) override;

    static QString nameForUrl(const QUrl& url);

    virtual Qn::ResourceStatus getStatus() const override;
    virtual void setStatus(Qn::ResourceStatus newStatus,
        Qn::StatusChangeReason reason = Qn::StatusChangeReason::Local) override;

private:
    // This class must not store its status on server side
    Qn::ResourceStatus m_status{Qn::NotDefined};
};

Q_DECLARE_METATYPE(QnWebPageResourcePtr)
Q_DECLARE_METATYPE(QnWebPageResourceList)
