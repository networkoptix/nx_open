#pragma once

#include <nx/fusion/model_functions_fwd.h>

#include "transaction/transaction.h"

namespace ec2 {
namespace migration {
namespace legacy {

struct PersistentInfoV1
{
    PersistentInfoV1() : sequence(0), timestamp(0) {}
    PersistentInfoV1& operator=(const PersistentInfoV1& ) = default;

    QnUuid dbID;
    qint32 sequence;
    qint64 timestamp;
};

#define QnAbstractTransaction_PERSISTENT_FieldsV1 (dbID)(sequence)(timestamp)
QN_FUSION_DECLARE_FUNCTIONS(PersistentInfoV1, (ubjson))


struct QnAbstractTransactionV1
{
    QnAbstractTransactionV1():
        command(ApiCommand::NotDefined),
        isLocal(false)
    {
    }

    ApiCommand::Value command;
    QnUuid peerID;
    PersistentInfoV1 persistentInfo;
    bool isLocal;
};
#define QnAbstractTransactionV1_Fields (command)(peerID)(persistentInfo)(isLocal)
QN_FUSION_DECLARE_FUNCTIONS(QnAbstractTransactionV1, (ubjson))

} // namespace legacy
} // namespace migration
} // namespace ec2
