/**********************************************************
* 1 nov 2014
* akolesnikov
***********************************************************/

#ifndef STREAM_TIME_SYNC_DATA_H
#define STREAM_TIME_SYNC_DATA_H

#include <QtCore/QMutex>

#include <utils/media/pts_to_clock_mapper.h>


class TimeSynchronizationData
{
public:
    int encoderThatInitializedThis;
    PtsToClockMapper::TimeSynchronizationData timeSyncData;
    QMutex mutex;

    TimeSynchronizationData();
    static TimeSynchronizationData* instance();
};

#endif  //STREAM_TIME_SYNC_DATA_H
