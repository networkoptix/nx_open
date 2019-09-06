#include "connection_speed.h"

#include <nx/fusion/model_functions.h>

namespace nx::hpm::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
	(ConnectionSpeed)(PeerConnectionSpeed),
	(json),
	_Fields)

bool ConnectionSpeed::operator==(const ConnectionSpeed& other) const
{
    return bandwidth == other.bandwidth && pingTime == other.pingTime;
}

std::string ConnectionSpeed::toString() const
{
    return QJson::serialized(*this).toStdString();
}

bool PeerConnectionSpeed::operator==(const PeerConnectionSpeed& other) const
{
    return
        systemId == other.systemId
        && serverId == other.serverId
        && connectionSpeed == other.connectionSpeed;
}

std::string PeerConnectionSpeed::toString() const
{
    return QJson::serialized(*this).toStdString();
}

} // namespace nx::hpm::api