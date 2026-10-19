// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms::client::desktop {

/**
 * Implements the macOS bundle entry point. The Client binary is the bundle main executable, so a
 * plain launch (Dock, Finder, URL or file open) is forwarded to the applauncher, which starts the
 * newest installed Client version passing "--run". Returns true if the launch was forwarded and
 * the process must exit. Otherwise removes "--run" from the arguments.
 */
bool forwardMacBundleLaunch(int* argc, char** argv);

} // namespace nx::vms::client::desktop
