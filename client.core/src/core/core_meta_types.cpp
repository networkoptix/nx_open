
#include "core_meta_types.h"

#include <core/system_connection_data.h>

void QnCoreMetaTypes::initialize()
{
    qRegisterMetaType<QnSystemConnectionData>();
    qRegisterMetaTypeStreamOperators<QnSystemConnectionData>();
    qRegisterMetaType<QnSystemConnectionDataList>();
    qRegisterMetaTypeStreamOperators<QnSystemConnectionDataList>();
}
