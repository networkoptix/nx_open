#include "uuid.h"

#if defined(_WIN32)
    #include <WinSock2.h>
#else
    #include <arpa/inet.h>
#endif

#include <nx/sdk/helpers/uuid_helper.h>

namespace nx {
namespace vms_server_plugins {
namespace utils {

QnUuid fromSdkUuidToQnUuid(const nx::sdk::Uuid& guid)
{
    return QnUuid(QUuid(
        ntohl(*((unsigned int*) &guid[0])),
        ntohs(*((unsigned short*) (&guid[4]))),
        ntohs(*((unsigned short*) (&guid[6]))),
        guid[8],
        guid[9],
        guid[10],
        guid[11],
        guid[12],
        guid[13],
        guid[14],
        guid[15]));
}

nx::sdk::Uuid fromQnUuidToSdkUuid(const QnUuid& uuid)
{
    return nx::sdk::UuidHelper::fromRawData(uuid.toRfc4122().data());
}

} // namespace utils
} // namespace vms_server_plugins
} // namespace nx
