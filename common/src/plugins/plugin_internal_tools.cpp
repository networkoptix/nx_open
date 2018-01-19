#include "plugin_internal_tools.h"
#include <nx/utils/log/log.h>
#include <plugins/plugin_tools.h>

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

nxpl::NX_GUID fromQnUuidToPluginGuid(const QnUuid& uuid)
{
    return nxpt::NxGuidHelper::fromRawData(uuid.toRfc4122());
}

} // namespace nxpt

namespace nx {
namespace sdk {

QString toString(const nx::sdk::ResourceInfo& resourceInfo)
{
    return lm(
        "Vendor: %1, Model: %2, Firmware: %3, UID: %4, Shared ID: %5, URL: %6, Channel: %7")
        .args(
            resourceInfo.vendor,
            resourceInfo.model,
            resourceInfo.firmware,
            resourceInfo.uid,
            resourceInfo.sharedId,
            resourceInfo.url,
            resourceInfo.channel).toQString();
}

} // namespace sdk
} // namespace nx
