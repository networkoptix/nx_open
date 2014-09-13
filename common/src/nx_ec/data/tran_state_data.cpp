#include "tran_state_data.h"
#include <utils/common/model_functions.h>

namespace ec2 {

#define QN_TRANSACTION_LOG_DATA_TYPES \
    (QnTranStateKey)\
    (QnTranState)\
    (QnTranStateResponse)\

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(QN_TRANSACTION_LOG_DATA_TYPES,  (json)(ubjson), _Fields)

}
