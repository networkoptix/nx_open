#include "client_core_meta_types.h"

#include <client_core/user_recent_connection_data.h>

void QnClientCoreMetaTypes::initialize()
{
    qRegisterMetaType<QnUserRecentConnectionData>();
    qRegisterMetaTypeStreamOperators<QnUserRecentConnectionData>();
    qRegisterMetaType<QnUserRecentConnectionDataList>();
    qRegisterMetaTypeStreamOperators<QnUserRecentConnectionDataList>();
}
