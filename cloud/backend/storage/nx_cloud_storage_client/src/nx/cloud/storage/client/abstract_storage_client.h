#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/url.h>

#include "abstract_content_client.h"
#include "abstract_metadata_client.h"

namespace nx::cloud::storage::client {

class NX_CLOUD_STORAGE_CLIENT_API AbstractStorageClient:
    public nx::network::aio::BasicPollable
{
public:
    virtual ~AbstractStorageClient() = default;

    /**
     * @param storageClientId Id of storage client. Should be unique for every client and
     *   preserved for every storage usage by the same client.
     * MUST be invoked prior to any other method.
     */
    virtual void open(
        const std::string& storageClientId,
        const nx::utils::Url& url,
        const nx::network::http::Credentials& credentials,
        nx::utils::MoveOnlyFunc<void(ResultCode)> handler) = 0;

    /**
     * NOTE: AbstractContentClient object is owned by AbstractStorageClient instance.
     */
    virtual AbstractContentClient* getContentClient() = 0;

    /**
     * NOTE: AbstractMetadataClient object is owned by AbstractStorageClient instance.
     */
    virtual AbstractMetadataClient* getMetadataClient() = 0;
};

} // namespace nx::cloud::storage::client
