// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_error.h"

namespace nx::vms::client::core {

RemoteConnectionError::RemoteConnectionError(RemoteConnectionErrorCode code):
    code(code)
{
}

bool RemoteConnectionError::operator==(RemoteConnectionErrorCode code) const
{
    return this->code == code;
}

} // namespace nx::vms::client::core
