#pragma once

#include <core/resource/resource.h>

class DummyResource: public QnResource
{
public:

    virtual QString getUniqueId() const { return QString(); }

    virtual Qn::ResourceStatus getStatus() const override
    {
        return Qn::Online;
    }

    virtual void setStatus(
        Qn::ResourceStatus newStatus,
        Qn::StatusChangeReason reason = Qn::StatusChangeReason::Local) override
    {
        Q_UNUSED(newStatus);
        Q_UNUSED(reason);
        //do nothing
    }
};