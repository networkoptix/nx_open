#include "transaction_transport_header.h"

#include <nx/fusion/model_functions.h>
#include "common/common_module.h"

namespace ec2
{
    static QAtomicInt qn_transportHeader_sequence(1);

    void QnTransactionTransportHeader::fillSequence(
        const QnUuid& moduleId,
        const QnUuid& runningInstanceGUID)
    {
        if (sequence == 0)
        {
            sequence = qn_transportHeader_sequence.fetchAndAddAcquire(1);
            sender = moduleId;
            senderRuntimeID = runningInstanceGUID;
        }
    }

    QString toString(const QnTransactionTransportHeader& header)
    {
        return lit("ttSeq=%1 sender=%2:%3").arg(header.sequence).arg(header.sender.toString()).arg(header.senderRuntimeID.toString());
    }

    bool QnTransactionTransportHeader::isNull() const
    {
        return senderRuntimeID.isNull();
    }

    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
        (QnTransactionTransportHeader), (ubjson)(json), _Fields, (optional, false))
}
