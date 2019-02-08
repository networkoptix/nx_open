#include "legacy_transaction_migration.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {
namespace migration {
namespace legacy {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    PersistentInfoV1,
    (ubjson),
    QnAbstractTransaction_PERSISTENT_FieldsV1,
    (optional, false))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnAbstractTransactionV1),
    (ubjson),
    _Fields,
    (optional, false))

} // namespace legacy
} // namespace migration
} // namespace ec2
