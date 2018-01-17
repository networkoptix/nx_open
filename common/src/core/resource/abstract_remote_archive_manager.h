#pragma once

#include <vector>
#include <stdint.h>
#include <limits>

#include <QtCore/QString>
#include <QtCore/QByteArray>

#include <recording/time_period_list.h>
#include <nx/streaming/abstract_archive_delegate.h>

namespace nx {
namespace core {
namespace resource {

using BufferType = QByteArray;
using OverlappedId = int;

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
    OverlappedId overlappedId = 0; //< Is needed for Hanwha driver.

    RemoteArchiveChunk() = default;
    RemoteArchiveChunk(
        QString id,
        int64_t startTime,
        int64_t duration,
        int overlappedId = 0)
        :
        id(id),
        startTimeMs(startTime),
        durationMs(duration),
        overlappedId(overlappedId)
    {
    }

    RemoteArchiveChunk(const RemoteArchiveChunk& other) = default;

    int64_t endTimeMs() const
    {
        return startTimeMs + durationMs;
    };
};

/**
 * Some general remote archive synchronization settings.
 */
struct RemoteArchiveSynchronizationSettings
{
    std::chrono::milliseconds waitBeforeSync;
    std::chrono::milliseconds waitBetweenChunks;
    int syncCyclesNumber;
    int triesPerPeriod;
};

inline bool operator==(const RemoteArchiveChunk& lhs, const RemoteArchiveChunk& rhs)
{
    return lhs.id == rhs.id
        && lhs.startTimeMs == rhs.startTimeMs
        && lhs.durationMs == rhs.durationMs;
}


using RemoteChunks = std::vector<RemoteArchiveChunk>;
using OverlappedRemoteChunks = std::map<OverlappedId, RemoteChunks>;
using OverlappedTimePeriods = std::map<OverlappedId, QnTimePeriodList>;

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
        OverlappedRemoteChunks* outArchiveEntries,
        int64_t startTimeMs = 0,
        int64_t endTimeMs = std::numeric_limits<int64_t>::max()) = 0;

    /**
     * Downloads specified entry content. Is used only if FullChunkCapability is present.
     * @return True if the whole entry has been successfully fetched to the buffer, false otherwise.
     */
    virtual bool fetchArchiveEntry(const QString& entryId, BufferType* outBuffer) = 0;

    /*
     * @return Archive delegate. Delegate is used only if StreamChunkCapability is present.
     */
    virtual std::unique_ptr<QnAbstractArchiveDelegate> archiveDelegate(
        const RemoteArchiveChunk& chunk) = 0;

    /**
     * Removes the specified entry from the device.
     * @return True if entry has been successfully deleted from the device, false otherwise.
     */
    virtual bool removeArchiveEntries(const std::vector<QString>& entryIds) = 0;

    /*
     * @return Remote archive capabilities.
     */
    virtual RemoteArchiveCapabilities capabilities() const = 0;

    /*
     * @return general synchronization settings.
     */
    virtual RemoteArchiveSynchronizationSettings settings() const = 0;

    /*
     * Is called before remote archive synchronization.
     */
    virtual void beforeSynchronization() = 0;

    /*
     * Is called after remote archive synchronization.
     */
    virtual void afterSynchronization(bool isSynchronizationSuccessful) = 0;
};

} // namespace resource
} // namespace core
} // namespace nx
