#include "transport_connection_info.h"

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/json_functions.h>

namespace ec2 {

QN_FUSION_ADAPT_STRUCT(QnTransportConnectionInfo, QnTransportConnectionInfo_Fields)
QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES((QnTransportConnectionInfo), (json))

} // namespace ec2