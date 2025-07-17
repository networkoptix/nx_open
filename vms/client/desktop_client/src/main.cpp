// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/build_info.h>
#include <nx/vms/client/desktop/application.h>
#include <nx/vms/client/desktop/ini.h>

int main(int argc, char** argv)
{
    return nx::vms::client::desktop::runApplication(argc, argv);
}
