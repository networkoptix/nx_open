#include "plugin_internal_tools.h"

#ifdef Q_OS_WIN
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif

namespace nxpt {

QnUuid fromPluginGuidToQnUuid(const nxpl::NX_GUID& guid)
{
    return QnUuid(QUuid(
        ntohl(*(unsigned int*)guid.bytes),
        ntohs(*(unsigned short*)(guid.bytes + 4)),
        ntohs(*(unsigned short*)(guid.bytes + 6)),
        guid.bytes[8],
        guid.bytes[9],
        guid.bytes[10],
        guid.bytes[11],
        guid.bytes[12],
        guid.bytes[13],
        guid.bytes[14],
        guid.bytes[15]));
}

} // namespace nxpt
