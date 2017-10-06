#pragma once

#include <QtCore/QString>

namespace nx {
namespace cloud {
namespace relaying {

class AppInfo
{
public:
    /** History name, used as a key in windows registry, appdata folder, etc. */
    static QString applicationName();

    /** Real application name, visible to the user. */
    static QString applicationDisplayName();
};

} // namespace relaying
} // namespace cloud
} // namespace nx
