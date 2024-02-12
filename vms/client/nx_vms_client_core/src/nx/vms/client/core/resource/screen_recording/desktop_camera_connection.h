// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <network/tcp_connection_processor.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/vms/client/core/resource/resource_fwd.h>

namespace nx::network::http {

class HttpClient;
class Credentials;

} // namespace nx::network::http

namespace nx::vms::client::core {

/**
 * Connection from desktop camera to a Server.
 */
class DesktopCameraConnection: public QnLongRunnable
{
public:
    typedef QnLongRunnable base_type;

    DesktopCameraConnection(
        DesktopResourcePtr owner,
        const QnMediaServerResourcePtr& server,
        const nx::Uuid& userId,
        const nx::Uuid& moduleGuid,
        const nx::network::http::Credentials& credentials);
    virtual ~DesktopCameraConnection();

    virtual void pleaseStop() override;
protected:
    virtual void run() override;
private:
    void terminatedSleep(int sleep);
    std::unique_ptr<nx::network::AbstractStreamSocket> takeSocketFromHttpClient(
        std::unique_ptr<nx::network::http::HttpClient>& httpClient);
private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

class DesktopCameraConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    DesktopCameraConnectionProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        void* sslContext,
        DesktopResourcePtr desktop);
    virtual ~DesktopCameraConnectionProcessor();
    void processRequest();
    void sendData(const nx::utils::ByteArray& data);
    void sendData(const char* data, int len);

    void sendUnlock();
    void sendLock();
    bool isConnected() const;
    QnResourcePtr getResource() const;
    void setServerVersion(const nx::utils::SoftwareVersion& version);
    nx::utils::SoftwareVersion serverVersion() const;
private:
    void disconnectInternal();
private:
    class Private;
    Private* d = nullptr;
};

} // namespace nx::vms::client::core
