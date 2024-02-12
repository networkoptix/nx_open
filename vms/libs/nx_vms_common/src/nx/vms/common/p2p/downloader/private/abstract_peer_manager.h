// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <future>
#include <memory>
#include <optional>

#include <api/server_rest_connection_fwd.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/common/p2p/downloader/file_information.h>

namespace nx::vms::common::p2p::downloader {

class NX_VMS_COMMON_API AbstractPeerManager: public QObject
{
public:
    enum Capability
    {
        FileInfo = 1 << 0,
        Checksums = 1 << 1,
        DownloadChunk = 1 << 2,
        AllCapabilities = FileInfo | Checksums | DownloadChunk
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    AbstractPeerManager(Capabilities capabilities, QObject* parent = nullptr):
        QObject(parent),
        capabilities(capabilities)
    {
    }

    virtual ~AbstractPeerManager() = default;

    template<typename T>
    struct BaseRequestContext
    {
        using Result = std::optional<T>;
        using Future = std::future<Result>;
        Future future;
        std::function<void()> cancelFunction = {};

        BaseRequestContext(Future future, std::function<void()> cancelFunction = {}) :
            future(std::move(future)),
            cancelFunction(cancelFunction)
        {}

        BaseRequestContext() = default;
        virtual ~BaseRequestContext() {}

        void cancel() const
        {
            if (cancelFunction)
                cancelFunction();
        }

        bool isValid() const
        {
            return future.valid();
        }
    };

    template<typename T>
    struct InternetRequestContext : BaseRequestContext<T>
    {
        InternetRequestContext(
            std::unique_ptr<nx::network::http::AsyncClient> httpClient,
            typename BaseRequestContext<T>::Future future,
            std::function<void()> cancelFunction = {})
            :
            BaseRequestContext<T>(std::move(future), cancelFunction),
            httpClient(std::move(httpClient))
        {}

        virtual ~InternetRequestContext()
        {
            if (httpClient)
                httpClient->pleaseStopSync();
        }

        InternetRequestContext() = default;
        std::unique_ptr<nx::network::http::AsyncClient> httpClient;
    };

    template<typename T>
    using RequestContextPtr = std::unique_ptr<BaseRequestContext<T>>;

    // All peer managers share promise between handler and canceller, and it is hard to guarantee
    // a certain call order. This function simplifies their code.
    template<typename T>
    static void setPromiseValueIfEmpty(
        const std::shared_ptr<std::promise<T>>& promise, const T& value)
    {
        try
        {
            promise->set_value(value);
        }
        catch (const std::future_error&)
        {
        }
    }

    /** @return Human readable peer name. This is mostly used in logs. */
    virtual QString peerString(const nx::Uuid& peerId) const { return peerId.toString(); }
    virtual QList<nx::Uuid> getAllPeers() const = 0;
    virtual QList<nx::Uuid> peers() const = 0;
    virtual int distanceTo(const nx::Uuid& peerId) const = 0;

    virtual RequestContextPtr<FileInformation> requestFileInfo(
        const nx::Uuid& peer, const QString& fileName, const nx::utils::Url& url) = 0;

    virtual RequestContextPtr<QVector<QByteArray>> requestChecksums(
        const nx::Uuid& peer, const QString& fileName) = 0;

    virtual RequestContextPtr<nx::Buffer> downloadChunk(
        const nx::Uuid& peerId,
        const QString& fileName,
        const nx::utils::Url& url,
        int chunkIndex,
        int chunkSize,
        qint64 fileSize) = 0;

    const Capabilities capabilities;
};

} // namespace nx::vms::common::p2p::downloader
