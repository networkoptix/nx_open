#ifndef QNTRANSPORTCONNECTIONINFO_H
#define QNTRANSPORTCONNECTIONINFO_H

#include "transaction_transport.h"

namespace ec2 {
    struct QnTransportConnectionInfo {
        bool incoming;
        QUrl url;
        QnUuid remotePeerId;
        QnTransactionTransport::State state;
    };
}

#endif // QNTRANSPORTCONNECTIONINFO_H
