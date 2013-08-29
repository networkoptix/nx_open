////////////////////////////////////////////////////////////
// 25 oct 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef PLUGINUSAGERECORD_H
#define PLUGINUSAGERECORD_H

#include <QtCore/QtCore>


static const quint64 USAGE_RECORD_EXPIRE_PERIOD = 1000;

class UsageRecord
{
public:
    quint32 sequence;
    //!millis since 1970/1/1
    quint64 updateClock;
    quint32 framePictureSize;
    double fps;
    quint64 pixelsPerSecond;
    quint64 videoMemoryUsage;
    quint32 simultaneousStreamCount;

    UsageRecord();

    bool serialize( quint8** const bufStart, const quint8* const bufEnd ) const;
    bool deserialize( const quint8** const bufStart, const quint8* const bufEnd );
};

class UsageRecordArray
{
public:
    quint32 prevUsageRecordID;
    std::vector<UsageRecord> records;

    UsageRecordArray();

    bool serialize( quint8** const bufStart, const quint8* const bufEnd ) const;
    bool deserialize( const quint8** const bufStart, const quint8* const bufEnd );
};

#endif  //PLUGINUSAGERECORD_H
