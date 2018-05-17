#pragma once

#include <core/resource/resource_fwd.h>

struct QnAbstractDesktopResourceSearcherImpl
{
    virtual ~QnAbstractDesktopResourceSearcherImpl() = default;
    virtual QnResourceList findResources() = 0;
};
