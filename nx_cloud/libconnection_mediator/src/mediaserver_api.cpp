#include "mediaserver_api.h"

#include "stun/custom_stun.h"
#include "stun/stun_message_dispatcher.h"

namespace nx {
namespace hpm {

MediaserverApiIf::~MediaserverApiIf()
{
}

bool MediaserverApi::ping( const SocketAddress& address, const QnUuid& expectedId )
{
    // TODO: implement
    return false;
}

} // namespace hpm
} // namespace nx
