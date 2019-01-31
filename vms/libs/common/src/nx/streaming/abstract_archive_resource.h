#ifndef ABSTRACT_ARCHIVE_RESOURCE_H
#define ABSTRACT_ARCHIVE_RESOURCE_H

#include <core/resource/media_resource.h>
#include <core/resource/resource.h>

class QnAbstractArchiveResource : public QnResource, public QnMediaResource
{
    Q_OBJECT;

public:
    QnAbstractArchiveResource();
    ~QnAbstractArchiveResource();

    //!Implementation of QnResource::getUniqueId
    virtual QString getUniqueId() const override;
    //!Implementation of QnResource::setUniqId
    virtual void setUniqId(const QString& value) override;

    //!Implementation of QnResource::setStatus
    virtual Qn::ResourceStatus getStatus() const override;
    virtual void setStatus(Qn::ResourceStatus newStatus, Qn::StatusChangeReason reason = Qn::StatusChangeReason::Local) override;

    //!Implementation of QnMediaResource::toResource
    virtual const QnResource* toResource() const override;
    //!Implementation of QnMediaResource::toResource
    virtual QnResource* toResource() override;
    //!Implementation of QnMediaResource::toResource
    virtual const QnResourcePtr toResourcePtr() const override;
    //!Implementation of QnMediaResource::toResource
    virtual QnResourcePtr toResourcePtr() override;

private:
    Qn::ResourceStatus m_localStatus;
};

#endif // ABSTRACT_ARCHIVE_RESOURCE_H
