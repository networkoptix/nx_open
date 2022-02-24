// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

struct QnAbstractDesktopResourceSearcherImpl
{
    virtual ~QnAbstractDesktopResourceSearcherImpl() = default;
    virtual QnResourceList findResources() = 0;
};
