#pragma once

#include <nx/utils/thread/mutex.h>

#include <network/tcp_connection_processor.h>

class QnHttpConnectionListener;

class Ec2ConnectionProcessor:
    public QnTCPConnectionProcessor
{
public:
    Ec2ConnectionProcessor(
        QSharedPointer<AbstractStreamSocket> socket,
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
};
