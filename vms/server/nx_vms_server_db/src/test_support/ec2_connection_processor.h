#pragma once

#include <nx/utils/thread/mutex.h>
#include <nx/vms/auth/abstract_nonce_provider.h>
#include <nx/vms/auth/abstract_user_data_provider.h>

#include <network/tcp_connection_processor.h>

class QnHttpConnectionListener;

class Ec2ConnectionProcessor:
    public QnTCPConnectionProcessor
{
public:
    Ec2ConnectionProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        nx::vms::auth::AbstractUserDataProvider* userDataProvider,
        nx::vms::auth::AbstractNonceProvider* nonceProvider,
        QnHttpConnectionListener* owner);

    virtual ~Ec2ConnectionProcessor();

protected:
    virtual void run() override;
    virtual void pleaseStop() override;
    bool processRequest(bool noAuth);
    bool authenticate();
    void addAuthHeader(nx::network::http::Response& response);

private:
    QnMutex m_mutex;
    QnTCPConnectionProcessor* m_processor = nullptr;
    nx::vms::auth::AbstractUserDataProvider* m_userDataProvider;
    nx::vms::auth::AbstractNonceProvider* m_nonceProvider;
};
