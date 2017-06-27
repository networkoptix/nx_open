#pragma once


#include "network/universal_request_processor.h"

class QnAudioProxyReceiverPrivate;

class QnAudioProxyReceiver: public QnTCPConnectionProcessor
{
public:
    QnAudioProxyReceiver(QSharedPointer<AbstractStreamSocket> socket, QnHttpConnectionListener* owner);

protected:
    virtual void run();
};
