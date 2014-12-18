#include "transaction_transport_header.h"

#include <utils/common/model_functions.h>
#include "common/common_module.h"

namespace ec2
{
    static QAtomicInt qn_transportHeader_sequence(1);

    void QnTransactionTransportHeader::fillSequence()
    {
        if (sequence == 0) {
            sequence = qn_transportHeader_sequence.fetchAndAddAcquire(1);
            sender = qnCommon->moduleGUID();
            senderRuntimeID = qnCommon->runningInstanceGUID();
        }
    }

    QString toString(const QnTransactionTransportHeader& header)
    {
        return lit("ttSeq=%1 sender=%2:%3").arg(header.sequence).arg(header.sender.toString()).arg(header.senderRuntimeID.toString());
    }

    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnTransactionTransportHeader, (json)(ubjson), QnTransactionTransportHeader_Fields);
}
