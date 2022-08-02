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

/**
 * Connection from desktop camera to a Server.
 */
class QnDesktopCameraConnection: public QnLongRunnable
{
public:
    typedef QnLongRunnable base_type;

    QnDesktopCameraConnection(
        QnDesktopResourcePtr owner,
        const QnMediaServerResourcePtr& server,
        const QnUuid& userId,
        const QnUuid& moduleGuid,
        const nx::network::http::Credentials& credentials);
    virtual ~QnDesktopCameraConnection();

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

class QnDesktopCameraConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    QnDesktopCameraConnectionProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        void* sslContext,
        QnDesktopResourcePtr desktop);
    virtual ~QnDesktopCameraConnectionProcessor();
    void processRequest();
    void sendData(const QnByteArray& data);
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
