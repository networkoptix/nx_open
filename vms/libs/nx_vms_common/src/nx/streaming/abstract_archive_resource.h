// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef ABSTRACT_ARCHIVE_RESOURCE_H
#define ABSTRACT_ARCHIVE_RESOURCE_H

#include <core/resource/media_resource.h>
#include <core/resource/resource.h>

class NX_VMS_COMMON_API QnAbstractArchiveResource: public QnMediaResource
{
    Q_OBJECT;

public:
    QnAbstractArchiveResource();
    ~QnAbstractArchiveResource();

    //!Implementation of QnResource::setStatus
    virtual nx::vms::api::ResourceStatus getStatus() const override;
    virtual void setStatus(nx::vms::api::ResourceStatus newStatus, Qn::StatusChangeReason reason = Qn::StatusChangeReason::Local) override;

    //!Implementation of QnMediaResource::toResource
    virtual const QnResource* toResource() const override;
    //!Implementation of QnMediaResource::toResource
    virtual QnResource* toResource() override;
    //!Implementation of QnMediaResource::toResource
    virtual const QnResourcePtr toResourcePtr() const override;
    //!Implementation of QnMediaResource::toResource
    virtual QnResourcePtr toResourcePtr() override;

private:
    nx::vms::api::ResourceStatus m_localStatus;
};

#endif // ABSTRACT_ARCHIVE_RESOURCE_H
