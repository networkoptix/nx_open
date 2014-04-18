struct TransactionTransportHeader
{
    TransactionTransportHeader(PeerList processedPeers = PeerList(), PeerList dstPeers = PeerList())
        : processedPeers(processedPeers), dstPeers(dstPeers) {}
    
    PeerList processedPeers;
    PeerList dstPeers;
};

QN_DEFINE_STRUCT_SERIALIZATORS(TransactionTransportHeader, (processedPeers) (dstPeers) );
