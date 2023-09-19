// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "globals_structures.h"

namespace nx::vms::client::desktop::jsapi {

Error Error::make(Globals::ErrorCode code,
    const QString& description)
{
    return Error{code, description};
}

Error Error::success()
{
    return make(Globals::success);
}

Error Error::failed(const QString& description)
{
    return make(Globals::failed, description);
}

Error Error::denied()
{
    return make(Globals::denied);
}

Error Error::invalidArguments(const QString& description)
{
    return make(Globals::invalid_args, description);
}

} // namespace nx::vms::client::desktop::jsapi
