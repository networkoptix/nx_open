
#include "core_meta_types.h"

#include <core/user_recent_connection_data.h>

void QnCoreMetaTypes::initialize()
{
    qRegisterMetaType<QnUserRecentConnectionData>();
    qRegisterMetaTypeStreamOperators<QnUserRecentConnectionData>();
    qRegisterMetaType<QnUserRecentConnectionDataList>();
    qRegisterMetaTypeStreamOperators<QnUserRecentConnectionDataList>();
}
