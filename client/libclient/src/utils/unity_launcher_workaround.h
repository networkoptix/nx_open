#pragma once

namespace nx {
namespace client {
namespace desktop {
namespace utils {

/*! In Ubuntu DE Unity Launcher makes some strange magic when determining application icons.
    It ignores the window icon set by an application itself and operates only
    on icons registered in the system with .desktop files. Sometimes its logic fails and it
    gives invalid icon to applications. I our case it assigns Network Manager icon in case if
    Desktop Client is started with a path containings substring "network " (with a space).
    This happens for every compatibility version which lives in
    "$HOME/.local/share/Network Optix/client/..."
    The simplest workaroud which is fouind so far is to avoid such substrings when starting
    the app. This can be achieved by starting the app as ./client-bin with the corresponding
    working directory. This class has startDetached function which does this for linux platform.
 */

class UnityLauncherWorkaround
{
public:
    static bool startDetached(const QString& program, const QStringList& arguments);
};

} // namespace utils
} // namespace desktop
} // namespace client
} // namespace nx
