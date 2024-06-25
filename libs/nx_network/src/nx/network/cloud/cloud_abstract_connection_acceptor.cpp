// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_abstract_connection_acceptor.h"

namespace nx::network::cloud {

bool AcceptorError::operator==(const AcceptorError& other) const
{
    return
        remoteAddress == other.remoteAddress &&
        sysErrorCode == other.sysErrorCode &&
        httpStatusCode == other.httpStatusCode;
}

bool AcceptorError::operator<(const AcceptorError& other) const
{
    return
        std::tie(remoteAddress, sysErrorCode, httpStatusCode)
        < std::tie(other.remoteAddress, other.sysErrorCode, other.httpStatusCode);
}

std::string AcceptorError::toString() const
{
    return nx::format("{remoteAddress: %1, sysErrorCode: %2, httpStatusCode: %3}",
        remoteAddress, sysErrorCode, httpStatusCode).toStdString();
}

} // namespace nx::network::cloud
