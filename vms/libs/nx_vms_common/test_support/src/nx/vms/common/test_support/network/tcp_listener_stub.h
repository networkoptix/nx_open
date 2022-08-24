// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class TcpListenerStub: public QnTcpListener
{
public:
    TcpListenerStub(QnCommonModule* commonModule) :
        QnTcpListener(commonModule, /*maxTcpRequestSize*/ 0, QHostAddress::Any, /*port*/ 0)
    {
    }

    virtual QnTCPConnectionProcessor* createRequestProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> /* clientSocket*/,
        int /*maxTcpRequestSize*/) override
    {
        return nullptr;
    }
};
