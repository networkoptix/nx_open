#pragma once

namespace nx::vms::server {

struct Chunk
{
    static const quint16 FILE_INDEX_NONE;
    static const quint16 FILE_INDEX_WITH_DURATION;
    static const int UnknownDuration;

    Chunk() = default;
    Chunk(
        qint64 _startTime, int _storageIndex, int _fileIndex, int _duration, qint16 _timeZone,
        quint16 fileSizeHi = 0, quint32 fileSizeLo = 0);

    qint64 distanceToTime(qint64 timeMs) const;
    qint64 endTimeMs() const;
    bool containsTime(qint64 timeMs) const;
    qint64 getFileSize() const { return ((qint64) fileSizeHi << 32) + fileSizeLo; }
    void setFileSize(qint64 value) { fileSizeHi = quint16(value >> 32); fileSizeLo = quint32(value); }
    bool isInfinite() const { return durationMs == -1; }

    QString fileName() const;

    qint64 startTimeMs = -1;
    int durationMs = 0;
    quint16 storageIndex = 0;
    quint16 fileIndex = 0;

    qint16 timeZone = -1;
    quint16 fileSizeHi = 0;
    quint32 fileSizeLo = 0;
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

    void truncate(qint64 timeMs);
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

} // namespace nx::vms::server
