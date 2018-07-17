#include "provider.h"

#include <nx/fusion/model_functions.h>

#include "../connection_manager.h"

namespace nx::data_sync_engine::statistics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json),
    _data_sync_engine_Fields)

//-------------------------------------------------------------------------------------------------

Provider::Provider(const ConnectionManager& connectionManager):
    m_connectionManager(connectionManager)
{
}

Statistics Provider::statistics() const
{
    Statistics data;
    data.connectedServerCountByVersion["all"] = m_connectionManager.getConnectionCount();
    return data;
}

} // namespace nx::data_sync_engine::statistics
