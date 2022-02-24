// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QMetaType>

namespace nx::vms::client::desktop {

/**
 * Struct that holds duration of archive and count of bookmarks to be backed up. Declared and
 * registered as metatype, so it may be passed as argument in queued connections.
 */
class BackupQueueSize
{
    Q_DECLARE_TR_FUNCTIONS(nx::vms::client::desktop::BackupQueueSize)

public:
    std::chrono::seconds duration;

    bool isEmpty() const;

    /**
     * @return String representation of remaining archive duration and bookmarks count which should
     *     be backed up. Duration part contains up two most valuable consecutive duration units
     *     with non-zero value, e.g. '7 weeks 5 days' or '1 hour 4 minutes'.
     */
    QString toString() const;
};

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::BackupQueueSize)
