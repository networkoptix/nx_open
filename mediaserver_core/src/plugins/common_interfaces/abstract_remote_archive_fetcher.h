#pragma once

#include <vector>
#include <stdint.h>
#include <limits>

#include <QtCore/QString>
#include <QtCore/QByteArray>

namespace nx {
namespace plugins {

using BufferType = QByteArray;

/**
 * Represents chunk in remote device archive.
 */
struct RemoteArchiveEntry 
{
    QString id;
    uint64_t startTime;
    uint64_t duration;
};

/**
 * Trait class that allows to control archive on remote device (e.g. on camera SD card).
 */
class AbstractRemoteArchiveFetcher
{
public:
    /**
     * Lists all entries on remote device that satisfies the given conditions.
     *
     * @param outArchiveEntries Vector of entries to be filled.
     *
     * @param startTimeMs List entries that starts not earlier than the specified timestamp.
     *
     * @param endTimeMs List entries that finishes till the specified timestamp.
     *
     * @return True if list of entries has been successfully fetched from the remote device, false otherwise.
     */
    virtual bool listAvailableArchiveEntries(
        std::vector<RemoteArchiveEntry>* outArchiveEntries,
        uint64_t startTimeMs = 0,
        uint64_t endTimeMs = std::numeric_limits<uint64_t>::max()) const = 0;

    /**
     * Downloads specified entry content.
     * @return True if the whole entry has been successfully fetched to the buffer, false otherwise.
     */
    virtual bool fetchArchiveEntry(const QString& entryId, BufferType* outBuffer) = 0;

    /**
     * Removes the specified entry from the device.
     * @return True if entry has been successfully deleted from the device, false otherwise. 
     */
    virtual bool removeArchiveEntry(const QString& entryId) = 0;
};

} // namespace plugins
} // namespace nx
