#pragma once

namespace nx
{
namespace media_db
{

#if defined(_DEBUG)
#define NX_DEBUG_ONLY_ASSERT(x) NX_ASSERT(x)
#else
#define NX_DEBUG_ONLY_ASSERT(x)
#endif

inline quint64 getBitMask(int width) { return (quint64)std::pow(2, width) - 1; }

enum class RecordType
{
    FileOperationAdd = 0,
    FileOperationDelete = 1,
    CameraOperationAdd = 2,
};

struct RecordBase
{
    quint64 part1;

    static const int kSerializedRecordSize = sizeof(part1);

    RecordBase(quint64 i = 0) : part1(i) {}

    RecordType getRecordType() const
    {
        return static_cast<RecordType>(part1 & 0x3);
    }

    void setRecordType(RecordType recordType)
    {
        part1 &= ~0x3;
        part1 |= (quint64)recordType & 0x3;
    }

    void setRecordTypeUnsafe(RecordType recordType)
    {
        NX_DEBUG_ONLY_ASSERT((int)getRecordType() == 0);
        part1 |= (quint64)recordType;
    }

};


} // namespace media_db
} // namespace nx
