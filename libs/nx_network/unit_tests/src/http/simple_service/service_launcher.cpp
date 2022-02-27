// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "service_launcher.h"

namespace nx::network::http::server::test {

bool Launcher::startService(std::vector<const char*> args /*={}*/, bool waitUntilStarted /*=true*/)
{
    for (const auto& arg: args)
        addArg(arg);

    if (waitUntilStarted)
        return startAndWaitUntilStarted();

    start();
    return true;
}

} // namespace nx::network::http::server::test
