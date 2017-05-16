#ifndef QN_TRANSACTION_TRANSPORT_HEADER_H
#define QN_TRANSACTION_TRANSPORT_HEADER_H

#include <nx_ec/ec_api.h>

#include <common/common_globals.h>
#include <nx/fusion/model_functions_fwd.h>

namespace ec2 {

    //enum TTHeaderFlag
    //{
    //    TT_None          = 0x0,
    //    TT_ProxyToClient = 0x1
    //};
    //QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(TTHeaderFlag)
    //Q_DECLARE_FLAGS(TTHeaderFlags, TTHeaderFlag)
    //Q_DECLARE_OPERATORS_FOR_FLAGS(TTHeaderFlags)

    struct QnTransactionTransportHeader
    {
        QnTransactionTransportHeader(): sequence(0), flags(Qn::TT_None), distance(0) {}
        QnTransactionTransportHeader(QnPeerSet processedPeers, QnPeerSet dstPeers = QnPeerSet()):
            processedPeers(processedPeers),
            dstPeers(dstPeers),
            sequence(0),
            flags(Qn::TT_None),
            distance(0) {}

        void fillSequence(
            const QnUuid& moduleId,
            const QnUuid& runningInstanceGUID);
        /** Calls previous method passing it value from \a commonModule(). */

        bool isNull() const;

        QnPeerSet processedPeers;
        QnPeerSet dstPeers;
        int sequence;
        QnUuid sender;
        QnUuid senderRuntimeID;
        Qn::TTHeaderFlags flags;
        int distance;
    };


#define QnTransactionTransportHeader_Fields (processedPeers)(dstPeers)(sequence)(sender)(senderRuntimeID)(flags)(distance)

    QN_FUSION_DECLARE_FUNCTIONS(QnTransactionTransportHeader, (ubjson)(json))

    QString toString(const QnTransactionTransportHeader& header);

} // namespace ec2

Q_DECLARE_METATYPE(ec2::QnTransactionTransportHeader);

#endif // QN_TRANSACTION_TRANSPORT_HEADER_H
