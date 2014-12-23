#ifndef QN_TRANSACTION_TRANSPORT_HEADER_H
#define QN_TRANSACTION_TRANSPORT_HEADER_H

#include <nx_ec/ec_api.h>

#include <utils/common/model_functions_fwd.h>

namespace ec2 {

    enum TTHeaderFlag
    {
        TT_None          = 0x0, 
        TT_ProxyToClient = 0x1
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(TTHeaderFlag)
    Q_DECLARE_FLAGS(TTHeaderFlags, TTHeaderFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(TTHeaderFlags)

    struct QnTransactionTransportHeader
    {
        QnTransactionTransportHeader(): sequence(0), flags(TT_None) {}
        QnTransactionTransportHeader(QnPeerSet processedPeers, QnPeerSet dstPeers = QnPeerSet()):
            processedPeers(processedPeers), 
            dstPeers(dstPeers), 
            sequence(0) {}

        void fillSequence();

        QnPeerSet processedPeers;
        QnPeerSet dstPeers;
        int sequence;
        QnUuid sender;
        QnUuid senderRuntimeID;
        TTHeaderFlags flags;
    };


#define QnTransactionTransportHeader_Fields (processedPeers)(dstPeers)(sequence)(sender)(senderRuntimeID)(flags)

    QN_FUSION_DECLARE_FUNCTIONS(QnTransactionTransportHeader, (ubjson))

    QString toString(const QnTransactionTransportHeader& header);

} // namespace ec2

#ifndef QN_NO_QT
Q_DECLARE_METATYPE(ec2::QnTransactionTransportHeader);
#endif

#endif // QN_TRANSACTION_TRANSPORT_HEADER_H
