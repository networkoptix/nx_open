// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "singleton.h"

#include <nx/utils/log/log.h>

void SingletonBase::printInstantiationError(const std::type_info& typeInfo)
{
    NX_ERROR(typeInfo, "Singleton is created more than once.");
}
