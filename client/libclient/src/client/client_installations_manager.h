#pragma once

#include <QtCore/QFileInfo>

class QnClientInstallationsManager
{
public:
    /** Get all paths where client might be installed. */
    static QStringList clientInstallRoots();

    /**
     * Get active launcher path. If all goes OK, it is minilauncher in Program Files.
     * Possibly it can also be old applauncher (if user has no admin rights).
     */
    static QFileInfo miniLauncher();
};
