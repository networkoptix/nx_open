// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QFile>

#include "http_async_client.h"

namespace nx::network::http {

/**
 * HTTP file downloader. Network operations are done asynchronously and file system operations
 * are done in the provided callbacks. The downloader can be used to download several files
 * sequentially with interruptions in the middle of the download.
 */
class NX_NETWORK_API AsyncFileDownloader:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    AsyncFileDownloader(ssl::AdapterFunc adapterFunc);

    virtual ~AsyncFileDownloader();

    AsyncFileDownloader(const AsyncFileDownloader&) = delete;
    AsyncFileDownloader& operator=(const AsyncFileDownloader&) = delete;

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    /**
     * Set the callback to call when the headers have been read.
     */
    void setOnResponseReceived(nx::utils::MoveOnlyFunc<void(std::optional<size_t>)> handler);

    /**
     * Set the callback to call when some data has been downloaded and saved.
     */
    void setOnProgressHasBeenMade(nx::utils::MoveOnlyFunc<void(nx::Buffer&&, std::optional<double>)> handler);

    /**
     * Set the callback to call when the download has been finished.
     */
    void setOnDone(nx::utils::MoveOnlyFunc<void()> handler);

    void setTimeouts(const AsyncClient::Timeouts& timeouts);
    void setAuthType(AuthType value);
    const Credentials& credentials() const;
    void setCredentials(const Credentials& credentials);
    void addAdditionalHeader(const std::string& key, const std::string& value);

    void setProxyCredentials(const Credentials& credentials);
    void setProxyVia(
        const SocketAddress& proxyEndpoint, bool isSecure, ssl::AdapterFunc adapterFunc);

    /**
     * Start a new download.
     * @param name file to save the received data.
     * @return .
     */
    void start(const nx::utils::Url& url, std::shared_ptr<QFile> file);

    std::string contentType() const;
    std::optional<size_t> contentLength() const;
    const Response* response() const;

    bool hasRequestSucceeded() const;
    bool failed() const;
    SystemError::ErrorCode lastSysErrorCode() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    enum class State
    {
        init,
        requesting,
        responseReceived,
        downloading,
        failedIo,
        done,
        stopped
    };

    nx::utils::MoveOnlyFunc<void(std::optional<size_t>)> m_onResponseReceived;
    nx::utils::MoveOnlyFunc<void(nx::Buffer&&, std::optional<double>)> m_onProgressHasBeenMade;
    nx::utils::MoveOnlyFunc<void()> m_onDone;

    State m_state = State::init;
    AsyncClient m_client;
    std::optional<size_t> m_contentLength;
    std::shared_ptr<QFile> m_file;

    void onResponseReceived();
    void onSomeMessageBodyAvailable();
    void onDone();
};

} // namespace nx::network::http
