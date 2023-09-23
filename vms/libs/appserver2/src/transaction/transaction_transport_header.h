// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_TRANSACTION_TRANSPORT_HEADER_H
#define QN_TRANSACTION_TRANSPORT_HEADER_H

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/api/data/peer_data.h>

namespace ec2 {

    enum TTHeaderFlag
    {
        TT_None          = 0x0,
        TT_ProxyToClient = 0x1
    };
    Q_DECLARE_FLAGS(TTHeaderFlags, TTHeaderFlag)

    struct QnTransactionTransportHeader
    {
        QnTransactionTransportHeader(): sequence(0), flags(ec2::TT_None), distance(0) {}
        QnTransactionTransportHeader(
            nx::vms::api::PeerSet processedPeers,
            nx::vms::api::PeerSet dstPeers = {})
            :
            processedPeers(processedPeers),
            dstPeers(dstPeers),
            sequence(0),
            flags(ec2::TT_None),
            distance(0) {}

        void fillSequence(
            const QnUuid& moduleId,
            const QnUuid& runningInstanceGUID);

        bool isNull() const;

        nx::vms::api::PeerSet processedPeers;
        nx::vms::api::PeerSet dstPeers;
        int sequence;
        QnUuid sender;
        QnUuid senderRuntimeID;
        int flags; //< TTHeaderFlags actually but declaring as int to simplify json representation.
        int distance;
    };


#define QnTransactionTransportHeader_Fields (processedPeers)(dstPeers)(sequence)(sender)(senderRuntimeID)(flags)(distance)

    QN_FUSION_DECLARE_FUNCTIONS(QnTransactionTransportHeader, (ubjson)(json))

    QString toString(const QnTransactionTransportHeader& header);

} // namespace ec2

#endif // QN_TRANSACTION_TRANSPORT_HEADER_H
