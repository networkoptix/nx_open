#pragma once

#include <vector>
#include <stdint.h>
#include <limits>

#include <QtCore/QString>
#include <QtCore/QByteArray>

namespace nx {
namespace core {
namespace resource {

using BufferType = QByteArray;

/**
 * Represents chunk in remote device archive.
 */
struct RemoteArchiveEntry 
{
    QString id;
    int64_t startTimeMs = 0;
    int64_t durationMs = 0;

    RemoteArchiveEntry() = default;
    RemoteArchiveEntry(QString _id, int64_t _startTime, int64_t _duration):
        id(_id),
        startTimeMs(_startTime),
        durationMs(_duration)
    {
    }
};

/**
 * Allows to control archive on remote device (e.g. on camera SD card).
 */
class AbstractRemoteArchiveManager
{
public:

    virtual ~AbstractRemoteArchiveManager() {};

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
        int64_t startTimeMs = 0,
        int64_t endTimeMs = std::numeric_limits<int64_t>::max()) = 0;

    /**
     * Downloads specified entry content.
     * @return True if the whole entry has been successfully fetched to the buffer, false otherwise.
     */
    virtual bool fetchArchiveEntry(const QString& entryId, BufferType* outBuffer) = 0;

    /**
     * Removes the specified entry from the device.
     * @return True if entry has been successfully deleted from the device, false otherwise. 
     */
    virtual bool removeArchiveEntries(const std::vector<QString>& entryIds) = 0;
};

} // namespace resource
} // namespace core
} // namespace nx
