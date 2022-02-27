// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QStringList>

namespace nx::vms::client::desktop {
namespace utils {

/*! In Ubuntu DE Unity Launcher sometimes can not correctly assign icons to applications. It
    ignores the window icon set by an application itself and operates only on icons registered in
    the system with .desktop files. Sometimes its logic fails and it gives an invalid icon to
    applications. In particular, it assigns the Network Manager icon to the application which has
    the "network " (with a space) substring in its path. Since our compatibility versions reside
    in "$HOME/.local/share/${customization.companyName}/client/..." and "companyName" variable for
    some customization contains "network "  this, is our case. The simplest workaround which is
    found so far is to avoid such substrings when starting the app. This can be achieved by
    starting the app as "./${customization.installerName}_client" in the corresponding working
    directory. This class has startDetached() function which does this for Linux platform.
 */

class UnityLauncherWorkaround
{
public:
    static bool startDetached(const QString& program, const QStringList& arguments, bool detachOutput = true);
};

} // namespace utils
} // namespace nx::vms::client::desktop
