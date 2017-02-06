/**********************************************************
* 1 nov 2014
* akolesnikov
***********************************************************/

#include "stream_time_sync_data.h"


TimeSynchronizationData::TimeSynchronizationData()
:
    encoderThatInitializedThis( -1 )
{
}

static TimeSynchronizationData TimeSynchronizationData_instance;

TimeSynchronizationData* TimeSynchronizationData::instance()
{
    return &TimeSynchronizationData_instance;
}
