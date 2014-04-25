#ifndef QN_TRANSACTION_TRANSPORT_HEADER_H
#define QN_TRANSACTION_TRANSPORT_HEADER_H

namespace ec2 {

    struct TransactionTransportHeader
    {
        TransactionTransportHeader(PeerList processedPeers, PeerList dstPeers = PeerList()): processedPeers(processedPeers), dstPeers(dstPeers) {}

        PeerList processedPeers;
        PeerList dstPeers;
    };
#define TransactionTransportHeader_Fields (processedPeers)(dstPeers)

} // namespace ec2

#endif // QN_TRANSACTION_TRANSPORT_HEADER_H
