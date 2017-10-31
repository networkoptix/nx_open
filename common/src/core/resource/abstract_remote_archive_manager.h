#pragma once

#include <vector>
#include <stdint.h>
#include <limits>

#include <QtCore/QString>
#include <QtCore/QByteArray>

#include <nx/streaming/abstract_archive_delegate.h>

namespace nx {
namespace core {
namespace resource {

using BufferType = QByteArray;

enum RemoteArchiveCapability
{
    NoCapabilities = 0,
    RemoveChunkCapability = 1,
    RandomAccessChunkCapability = 1 << 1
};

Q_DECLARE_FLAGS(RemoteArchiveCapabilities, RemoteArchiveCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(RemoteArchiveCapabilities)

/**
 * Represents chunk in remote device archive.
 */
struct RemoteArchiveChunk
{
    QString id;
    int64_t startTimeMs = 0;
    int64_t durationMs = 0;

    RemoteArchiveChunk() = default;
    RemoteArchiveChunk(QString _id, int64_t _startTime, int64_t _duration):
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
        std::vector<RemoteArchiveChunk>* outArchiveEntries,
        int64_t startTimeMs = 0,
        int64_t endTimeMs = std::numeric_limits<int64_t>::max()) = 0;

    /*
     * @param callback will be called when list of available entries is updated.
     */
    virtual void setOnAvailabaleEntriesUpdatedCallback(
        std::function<void(const std::vector<RemoteArchiveChunk>&)> callback) = 0;

    /**
     * Downloads specified entry content. Is used only if FullChunkCapability is present.
     * @return True if the whole entry has been successfully fetched to the buffer, false otherwise.
     */
    virtual bool fetchArchiveEntry(const QString& entryId, BufferType* outBuffer) = 0;

    /*
     * @return Archive delegate. Delegate is used only if StreamChunkCapability is present.
     */
    virtual std::unique_ptr<QnAbstractArchiveDelegate> archiveDelegate() = 0;

    /**
     * Removes the specified entry from the device.
     * @return True if entry has been successfully deleted from the device, false otherwise.
     */
    virtual bool removeArchiveEntries(const std::vector<QString>& entryIds) = 0;

    /*
     * @return Remote archive capabilities.
     */
    virtual RemoteArchiveCapabilities capabilities() const = 0;
};

} // namespace resource
} // namespace core
} // namespace nx
