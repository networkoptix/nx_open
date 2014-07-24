////////////////////////////////////////////////////////////
// 25 oct 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "pluginusagerecord.h"


template<class T>
static bool serialize( const T val, quint8** const bufStart, const quint8* const bufEnd )
{
    if( bufEnd <= *bufStart || (size_t)(bufEnd - *bufStart) < sizeof(val) )
        return false;
    memcpy( *bufStart, &val, sizeof(val) );
    *bufStart += sizeof(val);
    return true;
}

template<class T>
static bool deserialize( T* const val, const quint8** const bufStart, const quint8* const bufEnd )
{
    if( bufEnd <= *bufStart || (size_t)(bufEnd - *bufStart) < sizeof(*val) )
        return false;
    memcpy( val, *bufStart, sizeof(*val) );
    *bufStart += sizeof(*val);
    return true;
}

UsageRecord::UsageRecord()
:
    sequence( 0 ),
    updateClock( 0 ),
    framePictureSize( 0 ),
    fps( 0 ),
    pixelsPerSecond( 0 ),
    videoMemoryUsage( 0 ),
    simultaneousStreamCount( 0 )
{
}

bool UsageRecord::serialize( quint8** const bufStart, const quint8* const bufEnd ) const
{
    return 
        ::serialize( sequence, bufStart, bufEnd ) &&
        ::serialize( updateClock, bufStart, bufEnd ) &&
        ::serialize( framePictureSize, bufStart, bufEnd ) &&
        ::serialize( fps, bufStart, bufEnd ) &&
        ::serialize( pixelsPerSecond, bufStart, bufEnd ) &&
        ::serialize( videoMemoryUsage, bufStart, bufEnd ) &&
        ::serialize( simultaneousStreamCount, bufStart, bufEnd );
}

bool UsageRecord::deserialize( const quint8** const bufStart, const quint8* const bufEnd )
{
    return 
        ::deserialize( &sequence, bufStart, bufEnd ) &&
        ::deserialize( &updateClock, bufStart, bufEnd ) &&
        ::deserialize( &framePictureSize, bufStart, bufEnd ) &&
        ::deserialize( &fps, bufStart, bufEnd ) &&
        ::deserialize( &pixelsPerSecond, bufStart, bufEnd ) &&
        ::deserialize( &videoMemoryUsage, bufStart, bufEnd ) &&
        ::deserialize( &simultaneousStreamCount, bufStart, bufEnd );
}


UsageRecordArray::UsageRecordArray()
:
    prevUsageRecordID( 0 )
{
}

bool UsageRecordArray::serialize( quint8** const bufStart, const quint8* const bufEnd ) const
{
    if( !::serialize( prevUsageRecordID, bufStart, bufEnd ) )
        return false;
    if( !::serialize( (quint32)records.size(), bufStart, bufEnd ) )
        return false;

    for( std::vector<UsageRecord>::size_type
        i = 0;
        i < records.size();
        ++i )
    {
        if( !records[i].serialize( bufStart, bufEnd ) )
            return false;
    }

    return true;
}

bool UsageRecordArray::deserialize( const quint8** const bufStart, const quint8* const bufEnd )
{
    if( !::deserialize( &prevUsageRecordID, bufStart, bufEnd ) )
        return false;
    quint32 recordCount = 0;
    if( !::deserialize( &recordCount, bufStart, bufEnd ) )
        return false;

    const quint64 currentClock = QDateTime::currentMSecsSinceEpoch();

    records.clear();
    records.reserve( recordCount );
    for( std::vector<UsageRecord>::size_type
        i = 0;
        i < recordCount;
        ++i )
    {
        UsageRecord rec;
        if( !rec.deserialize( bufStart, bufEnd ) )
            return false;
        if( rec.updateClock + USAGE_RECORD_EXPIRE_PERIOD <= currentClock )
            continue;   //ignoring expired record
        records.push_back( rec );
    }

    return true;
}
