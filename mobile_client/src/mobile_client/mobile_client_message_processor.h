#ifndef MOBILE_CLIENT_MESSAGE_PROCESSOR_H
#define MOBILE_CLIENT_MESSAGE_PROCESSOR_H

#include <client/client_message_processor.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

class QnMobileClientMessageProcessor : public QnClientMessageProcessor {
    Q_OBJECT
    typedef QnClientMessageProcessor base_type;

public:
    QnMobileClientMessageProcessor();

    bool isConnected() const;

private:
};

#endif // MOBILE_CLIENT_MESSAGE_PROCESSOR_H
