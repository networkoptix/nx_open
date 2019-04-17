#include "chunk.h"

namespace nx::vms::server {

const quint16 Chunk::FILE_INDEX_NONE = 0xffff;
const quint16 Chunk::FILE_INDEX_WITH_DURATION = 0xfffe;
const int Chunk::UnknownDuration = -1;

Chunk(
    qint64 _startTime, int _storageIndex, int _fileIndex, int _duration, qint16 _timeZone,
    quint16 fileSizeHi = 0, quint32 fileSizeLo = 0)
    :
    startTimeMs(_startTime), durationMs(_duration), storageIndex(_storageIndex),
    fileIndex(_fileIndex), timeZone(_timeZone), fileSizeHi(fileSizeHi), fileSizeLo(fileSizeLo)
{
}

qint64 Chunk::distanceToTime(qint64 timeMs) const
{
    if (timeMs >= startTimeMs)
        return durationMs == -1 ? 0 : qMax(0ll, timeMs - (startTimeMs+durationMs));
    else
        return startTimeMs - timeMs;
}

qint64 Chunk::endTimeMs() const
{
    if (durationMs == -1)
        return DATETIME_NOW;
    else
        return startTimeMs + durationMs;
}

bool Chunk::containsTime(qint64 timeMs) const
{
    if (startTimeMs == -1)
        return false;
    else
        return qBetween(startTimeMs, timeMs, endTimeMs());
}

QString Chunk::fileName() const
{
    const auto basePart = fileIndex != FILE_INDEX_NONE && fileIndex != FILE_INDEX_WITH_DURATION
        ? strPadLeft(QString::number(fileIndex), 3, '0') : QString::number(startTimeMs)

    const auto durationPostfix = fileIndex == FILE_INDEX_WITH_DURATION
        ? "_" + QString::number(durationMs) : "";

    return basePart + durationPostfix + ".mkv";
}

void TruncableChunk::truncate(qint64 timeMs)
{
    if (!isTruncated)
    {
        originalDuration = durationMs;
        isTruncated = true;
    }
    durationMs = qMax(0ll, timeMs - startTimeMs);
}

} // namespace nx::vms::server
