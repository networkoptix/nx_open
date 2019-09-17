#include "chunk.h"

#include <nx/utils/datetime.h>
#include <utils/math/math.h>
#include <utils/common/util.h>

namespace nx::vms::server {

const uint16_t Chunk::FILE_INDEX_NONE = 0xffff;
const uint16_t Chunk::FILE_INDEX_WITH_DURATION = 0xfffe;
const int Chunk::UnknownDuration = -1;

Chunk::Chunk(
    int64_t startTimeMs, int storageIndex, int fileIndex, int _duration, int16_t timeZone,
    uint16_t fileSizeHi, uint32_t fileSizeLo)
    :
    startTimeMs(startTimeMs), durationMs(_duration), storageIndex(storageIndex),
    fileIndex(fileIndex), timeZone(timeZone), fileSizeHi(fileSizeHi), fileSizeLo(fileSizeLo)
{
}

int64_t Chunk::distanceToTime(int64_t timeMs) const
{
    if (timeMs >= startTimeMs)
        return durationMs == -1 ? 0 : std::max<int64_t>(0ll, timeMs - (startTimeMs+durationMs));
    else
        return startTimeMs - timeMs;
}

int64_t Chunk::endTimeMs() const
{
    if (durationMs == -1)
        return DATETIME_NOW;
    else
        return startTimeMs + durationMs;
}

bool Chunk::containsTime(int64_t timeMs) const
{
    if (startTimeMs == -1)
        return false;
    else
        return qBetween(startTimeMs, timeMs, endTimeMs());
}

QString Chunk::fileName() const
{
    const auto basePart = fileIndex != FILE_INDEX_NONE && fileIndex != FILE_INDEX_WITH_DURATION
        ? strPadLeft(QString::number(fileIndex), 3, '0') : QString::number(startTimeMs);

    const auto durationPostfix = fileIndex == FILE_INDEX_WITH_DURATION
        ? "_" + QString::number(durationMs) : "";

    return basePart + durationPostfix + ".mkv";
}

void TruncableChunk::truncate(int64_t timeMs)
{
    if (!isTruncated)
    {
        originalDuration = durationMs;
        isTruncated = true;
    }
    durationMs = std::max<int64_t>(0ll, timeMs - startTimeMs);
}

bool operator<(int64_t first, const Chunk& other)
{
    return first < other.startTimeMs;
}

bool operator<(const Chunk& first, int64_t other)
{
    return first.startTimeMs < other;
}

bool operator<(const Chunk& first, const Chunk& other)
{
    return first.startTimeMs < other.startTimeMs;
}

bool operator==(const Chunk& lhs, const Chunk& rhs)
{
    return lhs.startTimeMs == rhs.startTimeMs && lhs.durationMs == rhs.durationMs &&
        lhs.fileIndex == rhs.fileIndex && lhs.getFileSize() == rhs.getFileSize() &&
        lhs.timeZone == rhs.timeZone && lhs.storageIndex == rhs.storageIndex;
}

bool operator==(const UniqueChunk& lhs, const UniqueChunk& rhs)
{
    return lhs.chunk.toBaseChunk().startTimeMs == rhs.chunk.toBaseChunk().startTimeMs &&
        lhs.chunk.toBaseChunk().durationMs == rhs.chunk.toBaseChunk().durationMs &&
        lhs.cameraId == rhs.cameraId && lhs.quality == rhs.quality &&
        lhs.isBackup == rhs.isBackup;
}

bool operator<(const UniqueChunk& lhs, const UniqueChunk& rhs)
{
    if (lhs.cameraId != rhs.cameraId || lhs.quality != rhs.quality || lhs.isBackup != rhs.isBackup)
    {
        return lhs.cameraId < rhs.cameraId
            ? true : lhs.cameraId > rhs.cameraId
                ? false : lhs.quality < rhs.quality
                    ? true : lhs.quality > rhs.quality
                        ? false : lhs.isBackup < rhs.isBackup;
    }
    else
    {
        return lhs.chunk.startTimeMs < rhs.chunk.startTimeMs;
    }
}

} // namespace nx::vms::server
