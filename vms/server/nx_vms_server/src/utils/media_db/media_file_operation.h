#pragma once

#include "record_base.h"

namespace nx
{
namespace media_db
{

struct MediaFileOperation: RecordBase
{
    quint64 part2;

    static const int kSerializedRecordSize = sizeof(part1) + sizeof(part2);

    MediaFileOperation(quint64 i1 = 0, quint64 i2 = 0) : RecordBase(i1), part2(i2) {}

    int getCameraId() const
    {
        return (part1 >> 0x2) & 0xffff;
    }

    void setCameraId(int cameraId)
    {
        part1 &= ~(0xffffULL << 0x2);
        setCameraIdUnsafe((quint64)cameraId & 0xffff);
    }

    void setCameraIdUnsafe(quint64 cameraId)
    {
        NX_ASSERT_HEAVY_CONDITION(getCameraId() == 0);
        part1 |= cameraId << 0x2;
    }

    qint64 getStartTimeV1() const
    {   // -1 is a special value which designates that this operation
        // should be done with entire catalog instead of one chunk.
        qint64 ret = (part1 >> 0x12) & getBitMask(0x2a);
        return ret == 1 ? -1 : ret;
    }

    qint64 getStartTimeV2() const
    {   // -1 is a special value which designates that this operation
        // should be done with entire catalog instead of one chunk.
        qint64 ret = (part1 >> 0x12) & getBitMask(0x2a);
        return ret == getBitMask(0x2a) ? -1 : ret;
    }

    void setStartTime(qint64 startTime)
    {
        part1 &= ~(getBitMask(0x2a) << 0x12);
        return setStartTimeUnsafe(startTime);
    }

    void setStartTimeUnsafe(quint64 startTime)
    {
        NX_ASSERT_HEAVY_CONDITION(getStartTimeV2() == 0);
        part1 |= (startTime & getBitMask(0x2a)) << 0x12;
    }

    int getDuration() const
    {
        quint64 p1 = (part1 >> 0x3c) & 0xf;
        quint64 p2 = part2 & getBitMask(0x10);
        return p2 | (p1 << 0x10);
    }

    void setDuration(int duration)
    {
        part1 &= ~(0xfULL << 0x3c);
        part2 &= ~getBitMask(0x10);
        setDurationUnsafe(duration);
    }

    void setDurationUnsafe(int duration)
    {
        NX_ASSERT_HEAVY_CONDITION(getDuration() == 0);
        part1 |= (((quint64)duration >> 0x10) & 0xf) << 0x3c;
        part2 |= (quint64)duration & getBitMask(0x10);
    }

    int getTimeZoneV1() const
    {
        bool isPositive = ((part2 >> 0x10) & 0x1) == 0;
        int intPart = (part2 >> 0x11) & getBitMask(0x4);
        int remainder = (part2 >> 0x15) & 0x3;

        int result = intPart * 60;
        switch (remainder)
        {
        case 0:
            break;
        case 1:
            result += 30;
            break;
        case 2:
            result += 45;
            break;
        }
        return result * (isPositive ? 1 : -1);
    }

    int getTimeZoneV2() const
    {
        bool isPositive = ((part2 >> 0x10) & 0x1) == 0;
        const int intPart = (part2 >> 0x11) & getBitMask(0x6);
        return intPart * 15 * (isPositive ? 1 : -1);
    }

    void setTimeZone(int timeZone)
    {
        part2 &= ~(0x7fULL << 0x10);
        setTimeZoneUnsafe(timeZone);
    }

    void setTimeZoneUnsafe(int timeZone)
    {
        NX_ASSERT_HEAVY_CONDITION(getTimeZoneV2() == 0);

        if (timeZone < 0)
            part2 |= 0x1ULL << 0x10;

        const quint64 intPart = (qAbs(timeZone) / 15) & getBitMask(0x6);
        part2 |= intPart << 0x11;
    }

    qint64 getFileSize() const
    {
        return (part2 >> 0x17) & getBitMask(0x27LL);
    }

    const quint64 kFileSizeMaskLength = 0x27ULL;
    const quint64 kMaxFileSizeValue = getBitMask(kFileSizeMaskLength);

    void setFileSize(qint64 fileSize)
    {
        part2 &= ~(getBitMask(kFileSizeMaskLength) << 0x17);
        if (fileSize > kMaxFileSizeValue)
        {
            NX_WARNING(
                this,
                lm("File size value %1 would overflow. Setting to max available value %2 instead.")
                .arg(fileSize)
                .arg(kMaxFileSizeValue));
            part2 |= (getBitMask(kFileSizeMaskLength)) << 0x17;
            return;
        }
        setFileSizeUnsafe((quint64)fileSize & getBitMask(kFileSizeMaskLength));
    }

    void setFileSizeUnsafe(quint64 fileSize)
    {
        NX_ASSERT_HEAVY_CONDITION(getFileSize() == 0);
        NX_ASSERT_HEAVY_CONDITION(fileSize <= kMaxFileSizeValue);
        part2 |= fileSize << 0x17;
    }

    int getFileTypeIndex() const
    {
        return ((part2 >> 0x3e) & 0x1) == 0
            ? DeviceFileCatalog::Chunk::FILE_INDEX_WITH_DURATION
            : DeviceFileCatalog::Chunk::FILE_INDEX_NONE;
    }

    void setFileTypeIndex(int fileTypeIndex)
    {
        part2 &= ~(0x1ULL << 0x3e);
        setFileTypeIndexUnsafe(fileTypeIndex);
    }

    void setFileTypeIndexUnsafe(int fileTypeIndex)
    {
        NX_ASSERT_HEAVY_CONDITION(getFileTypeIndex() == DeviceFileCatalog::Chunk::FILE_INDEX_WITH_DURATION);
        NX_ASSERT_HEAVY_CONDITION(
            fileTypeIndex == DeviceFileCatalog::Chunk::FILE_INDEX_NONE
            || fileTypeIndex == DeviceFileCatalog::Chunk::FILE_INDEX_WITH_DURATION);

        const quint64 value = fileTypeIndex == DeviceFileCatalog::Chunk::FILE_INDEX_WITH_DURATION ? 0x0LL : 0x1LL;
        part2 |= value << 0x3e;
    }

    int getCatalog() const
    {
        return (part2 >> 0x3f) & 0x1;
    }

    void setCatalog(int catalog)
    {
        part2 &= ~(0x1ULL << 0x3f);
        setCatalogUnsafe(catalog & 0x1);
    }

    void setCatalogUnsafe(quint64 catalog)
    {
        NX_ASSERT_HEAVY_CONDITION(getCatalog() == 0);
        NX_ASSERT_HEAVY_CONDITION(catalog < 2);
        part2 |= catalog << 0x3f;
    }

};

} // namespace media_db
} // namespace nx
