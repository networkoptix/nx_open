#pragma once
#include "resource.h"

class LocalAudioFileResource : public QnResource
{
public:
    virtual QString getUniqueId() const { return QString(); }
    virtual void setStatus(Qn::ResourceStatus, bool /*silenceMode*/ ) override {}
    virtual Qn::ResourceStatus getStatus() const override { return Qn::Online; }
};
