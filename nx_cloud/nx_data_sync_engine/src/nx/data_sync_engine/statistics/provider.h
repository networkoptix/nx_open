#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx::data_sync_engine { class ConnectionManager; }

namespace nx::data_sync_engine::statistics {

struct Statistics
{
    std::map<std::string, int> connectedServerCountByVersion;
};

#define Statistics_data_sync_engine_Fields (connectedServerCountByVersion)

NX_DATA_SYNC_ENGINE_API bool deserialize(QnJsonContext*, const QJsonValue&, Statistics*);
NX_DATA_SYNC_ENGINE_API void serialize(QnJsonContext*, const Statistics&, class QJsonValue*);

//-------------------------------------------------------------------------------------------------

class NX_DATA_SYNC_ENGINE_API Provider
{
public:
    Provider(const ConnectionManager& connectionManager);

    Statistics statistics() const;

private:
    const ConnectionManager& m_connectionManager;
};

} // namespace nx::data_sync_engine::statistics
