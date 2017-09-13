#pragma once

namespace nx {
namespace cloud {
namespace relay {

class AppInfo
{
public:
    /** History name, used as a key in windows registry, appdata folder, etc. */
    static QString applicationName();

    /** Real application name, visible to the user. */
    static QString applicationDisplayName();
};

} // namespace relay
} // namespace cloud
} // namespace nx
