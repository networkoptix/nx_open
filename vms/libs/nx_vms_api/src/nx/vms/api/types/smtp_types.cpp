#include "smtp_types.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, ConnectionType,
    (nx::vms::api::ConnectionType::unsecure, "Unsecure")
    (nx::vms::api::ConnectionType::ssl, "Ssl")
    (nx::vms::api::ConnectionType::tls, "Tls"))
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::ConnectionType, (numeric)(debug))
