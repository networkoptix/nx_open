#include "api_peer_alive_data.h"

#include <utils/common/model_functions.h>

namespace ec2 {
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiPeerAliveData), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields, (optional, true))
} // namespace ec2

void serialize_field(const ec2::ApiPeerAliveData &, QVariant *) { return; }
void deserialize_field(const QVariant &, ec2::ApiPeerAliveData *) { return; }

void serialize_field(const ec2::ApiPeerData &, QVariant *) { return; }
void deserialize_field(const QVariant &, ec2::ApiPeerData *) { return; }
