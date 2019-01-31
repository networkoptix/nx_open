#include "uuid.h"

#if defined(_WIN32)
    #include <WinSock2.h>
#else
    #include <arpa/inet.h>
#endif

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>
#include <plugins/plugin_tools.h>

namespace nx {
namespace vms_server_plugins {
namespace utils {

QnUuid fromPluginGuidToQnUuid(const nxpl::NX_GUID& guid)
{
    return QnUuid(QUuid(
        ntohl(*((unsigned int*) guid.bytes)),
        ntohs(*((unsigned short*) (guid.bytes + 4))),
        ntohs(*((unsigned short*) (guid.bytes + 6))),
        guid.bytes[8],
        guid.bytes[9],
        guid.bytes[10],
        guid.bytes[11],
        guid.bytes[12],
        guid.bytes[13],
        guid.bytes[14],
        guid.bytes[15]));
}

nxpl::NX_GUID fromQnUuidToPluginGuid(const QnUuid& uuid)
{
    return nxpt::NxGuidHelper::fromRawData(uuid.toRfc4122());
}

} // namespace utils
} // namespace vms_server_plugins
} // namespace nx
