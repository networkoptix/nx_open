#pragma once

namespace nx
{
namespace media_db
{

constexpr quint64 getBitMask(int width) 
{ 
    return (quint64) (1LL << width) - 1; 
}

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
        NX_ASSERT_HEAVY_CONDITION((int)getRecordType() == 0);
        part1 |= (quint64)recordType;
    }

};


} // namespace media_db
} // namespace nx
