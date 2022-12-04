// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_error.h"

#include <nx/reflect/to_string.h>

namespace nx::vms::client::core {

RemoteConnectionError::RemoteConnectionError(RemoteConnectionErrorCode code):
    code(code)
{
}

bool RemoteConnectionError::operator==(RemoteConnectionErrorCode code) const
{
    return this->code == code;
}

QString RemoteConnectionError::toString() const
{
    QString result = QString::fromStdString(nx::reflect::toString(code));
    if (externalDescription)
        result += "(" + *externalDescription + ")";
    return result;
}

} // namespace nx::vms::client::core
