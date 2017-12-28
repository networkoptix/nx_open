#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/abstract_msg_body_source.h>
#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/utils/service.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/stoppable.h>

#include <nx/cloud/cdb/api/result_code.h>
#include <nx/utils/stree/resourcecontainer.h>
#include <nx/utils/db/async_sql_query_executor.h>

#include "access_control/auth_types.h"
#include "managers/managers_types.h"
#include "settings.h"


class QnCommandLineParser;

namespace nx {
namespace network {
namespace http {

class MessageDispatcher;

} // namespace nx
} // namespace network
} // namespace http

namespace nx {

namespace db { class AsyncSqlQueryExecutor; }
namespace utils { class TimerManager; }

namespace cdb {

class AbstractEmailManager;
class StreeManager;
class TemporaryAccountPasswordManager;
class AccountManager;
class EventManager;
class SystemManager;
class SystemHealthInfoProvider;
class AuthenticationManager;
class AuthorizationManager;
class AuthenticationProvider;
class MaintenanceManager;
class CloudModuleUrlProvider;
class HttpView;

namespace conf { class Settings; }
namespace ec2 { class ConnectionManager; }

class CloudDbService:
    public nx::utils::Service
{
    using base_type = nx::utils::Service;

public:
    CloudDbService(int argc, char **argv);

    std::vector<network::SocketAddress> httpEndpoints() const;

protected:
    virtual std::unique_ptr<utils::AbstractServiceSettings> createSettings() override;
    virtual int serviceMain(const utils::AbstractServiceSettings& settings) override;

private:
    const conf::Settings* m_settings;
    HttpView* m_view = nullptr;
};

} // namespace cdb
} // namespace nx
