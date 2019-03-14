#pragma once

#include "network/universal_request_processor.h"

class QnAudioProxyReceiverPrivate;
class QnMediaServerModule;

class QnAudioProxyReceiver: public QnTCPConnectionProcessor
{
public:
    QnAudioProxyReceiver(
        QnMediaServerModule* serverModule,
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnHttpConnectionListener* owner);

protected:
    virtual void run();
private:
    QnMediaServerModule* m_serverModule = nullptr;
};
