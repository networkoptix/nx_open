#pragma once

#include <memory>
#include <string>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/move_only_func.h>

namespace nx::data_sync_engine {

class AbstractTransactionTransport;

class AbstractTransactionTransportConnector:
    public network::aio::BasicPollable
{
public:
    using Handler = nx::utils::MoveOnlyFunc<void(
        std::unique_ptr<AbstractTransactionTransport> /*connection*/)>;

    virtual ~AbstractTransactionTransportConnector() = default;

    virtual void connect(Handler completionHandler) = 0;
    virtual std::string lastErrorText() const = 0;
};

} // namespace nx::data_sync_engine
