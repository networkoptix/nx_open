// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/test_support/module_instance_launcher.h>

#include "simple_service.h"

namespace nx::network::http::server::test {

class Launcher: public nx::utils::test::ModuleLauncher<SimpleService>
{
public:
    bool startService(std::vector<const char*> args = {}, bool waitUntilStarted = true);
};

} // namespace nx::network::http::server::test
