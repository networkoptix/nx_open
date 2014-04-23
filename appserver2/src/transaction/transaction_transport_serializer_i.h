struct TransactionTransportHeader
{
    TransactionTransportHeader(PeerList processedPeers, PeerList dstPeers = PeerList()): processedPeers(processedPeers), dstPeers(dstPeers) {}
    PeerList processedPeers;
    PeerList dstPeers;
};

//QN_DEFINE_STRUCT_SERIALIZATORS(TransactionTransportHeader, (processedPeers) (dstPeers) );
QN_FUSION_DECLARE_FUNCTIONS(TransactionTransportHeader, (binary))
