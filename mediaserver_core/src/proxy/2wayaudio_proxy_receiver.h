#pragma once


#include "network/universal_request_processor.h"
#include "proxy_connection.h"

class QnTwoWayAudioProxyReceiverConnectionPrivate;

class QnTwoWayAudioProxyReceiverConnection: public QnTCPConnectionProcessor
{
public:
    QnTwoWayAudioProxyReceiverConnection(QSharedPointer<AbstractStreamSocket> socket,
                                         QnHttpConnectionListener* owner);
    virtual bool isTakeSockOwnership() const override;
protected:
    virtual void run();
private:
    Q_DECLARE_PRIVATE(QnTwoWayAudioProxyReceiverConnection);
};
