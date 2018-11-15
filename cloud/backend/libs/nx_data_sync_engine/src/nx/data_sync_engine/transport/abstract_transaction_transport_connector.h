#pragma once

#include <memory>
#include <string>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/move_only_func.h>

namespace nx::clusterdb::engine::transport {

class AbstractCommandPipeline;
class AbstractConnection;

struct ConnectResultDescriptor
{
    SystemError::ErrorCode systemResultCode = SystemError::noError;
    nx::network::http::StatusCode::Value httpStatusCode =
        nx::network::http::StatusCode::ok;

    ConnectResultDescriptor() = default;

    ConnectResultDescriptor(SystemError::ErrorCode systemResultCode):
        systemResultCode(systemResultCode)
    {
    }

    ConnectResultDescriptor(nx::network::http::StatusCode::Value httpStatusCode):
        httpStatusCode(httpStatusCode)
    {
    }

    ConnectResultDescriptor(
        SystemError::ErrorCode systemResultCode,
        nx::network::http::StatusCode::Value httpStatusCode)
        :
        systemResultCode(systemResultCode),
        httpStatusCode(httpStatusCode)
    {
    }

    bool ok() const
    {
        return systemResultCode == SystemError::noError
            && nx::network::http::StatusCode::isSuccessCode(httpStatusCode);
    }

    std::string toString() const
    {
        // TODO
        return "";
    }
};

//-------------------------------------------------------------------------------------------------

class AbstractCommandPipelineConnector:
    public network::aio::BasicPollable
{
public:
    using Handler = nx::utils::MoveOnlyFunc<void(
        ConnectResultDescriptor connectResultDescriptor,
        std::unique_ptr<AbstractCommandPipeline> /*connection*/)>;

    virtual ~AbstractCommandPipelineConnector() = default;

    virtual void connect(Handler completionHandler) = 0;
};

//-------------------------------------------------------------------------------------------------

class AbstractTransactionTransportConnector:
    public network::aio::BasicPollable
{
public:
    using Handler = nx::utils::MoveOnlyFunc<void(
        ConnectResultDescriptor connectResultDescriptor,
        std::unique_ptr<AbstractConnection> /*connection*/)>;

    virtual ~AbstractTransactionTransportConnector() = default;

    virtual void connect(Handler completionHandler) = 0;
};

} // namespace nx::clusterdb::engine::transport
