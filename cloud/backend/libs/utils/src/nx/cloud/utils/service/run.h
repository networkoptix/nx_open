#pragma once

#include <QCoreApplication>

#include <nx/network/socket_global.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/scope_guard.h>

namespace nx::cloud::utils::service {

/**
 * Creates a QCoreApplication with argc and argv, disables udt in SocketGlobals, 
 * enables stacktrace on assertion, and invokes func with no arguments. 
 * func should be bound with all arguments needed to run it and should return an int.
 */
template<typename CloudServiceFunc>
int run(int argc, char* argv[], CloudServiceFunc func)
{
    QCoreApplication app(argc, argv);

    nx::network::SocketGlobals::InitGuard sgGuard(
        nx::network::InitializationFlags::disableUdt);

    nx::utils::printStackTraceOnAssert(true);

    return func();
}

} // namespace nx::cloud::utils::service