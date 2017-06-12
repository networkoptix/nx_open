#pragma once

class TcpListenerStub: public QnTcpListener
{
public:
    TcpListenerStub(QnCommonModule* commonModule) :
        QnTcpListener(commonModule, QHostAddress::Any, /*port*/ 0)
    {
    }

    virtual QnTCPConnectionProcessor* createRequestProcessor(
        QSharedPointer<AbstractStreamSocket> clientSocket) override
    {
        return nullptr;
    }
};
