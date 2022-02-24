// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QFileInfo>

class QnClientInstallationsManager
{
public:
    /** Binaries directory location relative to client package root. */
    static QString binDirSuffix();

    /** lib directory location relative to client package root. */
    static QString libDirSuffix();

    /** Get all paths where client might be installed. */
    static QStringList clientInstallRoots();

    /**
     * Get active minilauncher path. If all goes OK, it is minilauncher in Program Files.
     * Possibly it can also be old applauncher (if user has no admin rights).
     */
    static QFileInfo miniLauncher();

    /**
    * Get active applauncher path. If all goes OK, it is applauncher in AppData/Local.
    */
    static QFileInfo appLauncher();
};
