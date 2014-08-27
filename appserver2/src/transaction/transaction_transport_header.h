#ifndef QN_TRANSACTION_TRANSPORT_HEADER_H
#define QN_TRANSACTION_TRANSPORT_HEADER_H

#include <nx_ec/ec_api.h>

#include <utils/common/model_functions_fwd.h>

namespace ec2 {

    struct QnTransactionTransportHeader
    {
        QnTransactionTransportHeader(): sequence(0) {}
        QnTransactionTransportHeader(QnPeerSet processedPeers, QnPeerSet dstPeers = QnPeerSet(), int sequence = 0):
            processedPeers(processedPeers), 
            dstPeers(dstPeers), 
            sequence(sequence) {}

        void fillSequence();

        QnPeerSet processedPeers;
        QnPeerSet dstPeers;
        int sequence;
        QUuid sender;
    };
#define QnTransactionTransportHeader_Fields (processedPeers)(dstPeers)(sequence)(sender)

    QN_FUSION_DECLARE_FUNCTIONS(QnTransactionTransportHeader, (binary)(json)(ubjson))

} // namespace ec2

#ifndef QN_NO_QT
Q_DECLARE_METATYPE(ec2::QnTransactionTransportHeader);
#endif

#endif // QN_TRANSACTION_TRANSPORT_HEADER_H
