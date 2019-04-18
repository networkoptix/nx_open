#pragma once

#include <set>
#include <cstdint>
#include <server/server_globals.h>

namespace nx::vms::server {

struct Chunk
{
    static const uint16_t FILE_INDEX_NONE;
    static const uint16_t FILE_INDEX_WITH_DURATION;
    static const int UnknownDuration;

    Chunk() = default;
    Chunk(
        int64_t startTimeMs, int storageIndex, int fileIndex, int duration, int16_t timeZone,
        quint16 fileSizeHi = 0, quint32 fileSizeLo = 0);

    int64_t distanceToTime(int64_t timeMs) const;
    int64_t endTimeMs() const;
    bool containsTime(int64_t timeMs) const;
    int64_t getFileSize() const { return ((int64_t) fileSizeHi << 32) + fileSizeLo; }
    void setFileSize(int64_t value) { fileSizeHi = uint16_t(value >> 32); fileSizeLo = quint32(value); }
    bool isInfinite() const { return durationMs == -1; }

    QString fileName() const;

    int64_t startTimeMs = -1;
    int durationMs = 0;
    uint16_t storageIndex = 0;
    uint16_t fileIndex = 0;

    int16_t timeZone = -1;
    uint16_t fileSizeHi = 0;
    uint32_t fileSizeLo = 0;
};

struct TruncableChunk : Chunk
{
    using Chunk::Chunk;
    int64_t originalDuration;
    bool isTruncated;

    TruncableChunk()
        :
        Chunk(),
        originalDuration(0),
        isTruncated(false)
    {}

    TruncableChunk(const Chunk &other)
        :
        Chunk(other),
        originalDuration(other.durationMs),
        isTruncated(false)
    {}

    Chunk toBaseChunk() const
    {
        Chunk ret(*this);
        if (originalDuration != 0)
            ret.durationMs = originalDuration;
        return ret;
    }

    void truncate(int64_t timeMs);
};

struct UniqueChunk
{
    TruncableChunk              chunk;
    QString                     cameraId;
    QnServer::ChunksCatalog     quality;
    bool                        isBackup;

    UniqueChunk(
        const TruncableChunk        &chunk,
        const QString               &cameraId,
        QnServer::ChunksCatalog     quality,
        bool                        isBackup
        )
        : chunk(chunk),
        cameraId(cameraId),
        quality(quality),
        isBackup(isBackup)
    {}
};

typedef std::set<UniqueChunk> UniqueChunkCont;

bool operator<(const Chunk& first, const Chunk& other);
bool operator<(int64_t first, const Chunk& other);
bool operator <(const Chunk& other, int64_t first);
bool operator==(const Chunk& lhs, const Chunk& rhs);

bool operator==(const UniqueChunk& lhs, const UniqueChunk& rhs);
bool operator<(const UniqueChunk& lhs, const UniqueChunk& rhs);

} // namespace nx::vms::server
