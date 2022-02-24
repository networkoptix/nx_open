// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once
#include "resource.h"

class LocalAudioFileResource : public QnResource
{
public:
    virtual void setStatus(nx::vms::api::ResourceStatus, Qn::StatusChangeReason /*reason*/ ) override {}
    virtual nx::vms::api::ResourceStatus getStatus() const override { return nx::vms::api::ResourceStatus::online; }
};
