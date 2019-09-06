#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/move_only_func.h>

#include "result_code.h"

namespace nx::cloud::storage::client {

struct MetadataRecord
{
    // TODO
};

class NX_CLOUD_STORAGE_CLIENT_API AbstractMetadataClient:
    public nx::network::aio::BasicPollable
{
public:
    using OpenHandler = nx::utils::MoveOnlyFunc<void(ResultCode)>;
    using ErrorHandler = nx::utils::MoveOnlyFunc<void(ResultCode)>;

    virtual void open(OpenHandler handler) = 0;

    /**
     * The data can be buffered inside and uploaded with some delay.
     * If an error happens while upoading, then upload is retried.
     * NOTE: Can be invoked only after successful AbstractMetadataClient::open completion.
     */
    virtual void save(MetadataRecord data) = 0;

    /**
     * @param handler Invoked on some error.
     * NOTE: An error does not stop upload attempts.
     */
    virtual void setOnError(ErrorHandler handler) = 0;

    // TODO
};

} // namespace nx::cloud::storage::client
