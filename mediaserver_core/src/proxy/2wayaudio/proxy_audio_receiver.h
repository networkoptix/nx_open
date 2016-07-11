#pragma once


#include "network/universal_request_processor.h"

class QnAudioProxyReceiverPrivate;

class QnAudioProxyReceiver: public QnTCPConnectionProcessor
{
public:
    QnAudioProxyReceiver(QSharedPointer<AbstractStreamSocket> socket,
                                         QnHttpConnectionListener* owner);
    virtual bool isTakeSockOwnership() const override;
protected:
    virtual void run();
private:
    Q_DECLARE_PRIVATE(QnAudioProxyReceiver);
};
