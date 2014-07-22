#ifndef QN_TRANSACTION_TRANSPORT_HEADER_H
#define QN_TRANSACTION_TRANSPORT_HEADER_H

#include <nx_ec/ec_api.h>

#include <utils/common/model_functions_fwd.h>

namespace ec2 {

    struct QnTransactionTransportHeader
    {
        QnTransactionTransportHeader() {}
        QnTransactionTransportHeader(QnPeerSet processedPeers, QnPeerSet dstPeers = QnPeerSet()): processedPeers(processedPeers), dstPeers(dstPeers) {}

        QnPeerSet processedPeers;
        QnPeerSet dstPeers;
    };
#define QnTransactionTransportHeader_Fields (processedPeers)(dstPeers)

    QN_FUSION_DECLARE_FUNCTIONS(QnTransactionTransportHeader, (binary)(json)(ubj))

} // namespace ec2

#ifndef QN_NO_QT
Q_DECLARE_METATYPE(ec2::QnTransactionTransportHeader);
#endif

#endif // QN_TRANSACTION_TRANSPORT_HEADER_H
