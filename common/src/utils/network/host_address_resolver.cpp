/**********************************************************
* 20 jan 2015
* a.kolesnikov
***********************************************************/

#include "host_address_resolver.h"


bool HostAddressResolver::resolveAddressAsync(
    const HostAddress& addressToResolve,
    std::function<void (SystemError::ErrorCode, const HostAddress&)>&& completionHandler,
    RequestID reqID )
{
    return false;
}

bool HostAddressResolver::resolveAddressSync( const QString& hostName, HostAddress* const resolvedAddress )
{
    return false;
}

bool HostAddressResolver::isAddressResolved( const HostAddress& addr ) const
{
    return false;
}

void HostAddressResolver::cancel( RequestID reqID, bool waitForRunningHandlerCompletion )
{
}

bool HostAddressResolver::isRequestIDKnown( RequestID reqID ) const
{
    return false;
}
