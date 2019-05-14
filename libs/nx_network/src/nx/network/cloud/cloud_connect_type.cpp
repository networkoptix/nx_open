#include "cloud_connect_type.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::network::cloud, ConnectType,
    (nx::network::cloud::ConnectType::unknown, "unknown")
    (nx::network::cloud::ConnectType::forwardedTcpPort, "tcp")
    (nx::network::cloud::ConnectType::udpHp, "udpHp")
    (nx::network::cloud::ConnectType::tcpHp, "tcpHp")
    (nx::network::cloud::ConnectType::proxy, "proxy")
    (nx::network::cloud::ConnectType::all, "all")
)
