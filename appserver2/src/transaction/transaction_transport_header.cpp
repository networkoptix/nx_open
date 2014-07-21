#include "transaction_transport_header.h"

#include <utils/common/model_functions.h>

namespace ec2
{
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnTransactionTransportHeader, (binary)(json)(ubj), QnTransactionTransportHeader_Fields);
}