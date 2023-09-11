// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource.h>

class DummyResource: public QnResource
{
public:
    virtual nx::vms::api::ResourceStatus getStatus() const override
    {
        return nx::vms::api::ResourceStatus::online;
    }

    virtual void setStatus(
        nx::vms::api::ResourceStatus /*newStatus*/,
        Qn::StatusChangeReason /*reason*/ = Qn::StatusChangeReason::Local) override
    {
    }
};
