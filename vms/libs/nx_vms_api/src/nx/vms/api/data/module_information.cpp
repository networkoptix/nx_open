#include "module_information.h"

#include <QtNetwork/QNetworkInterface>
#include <QtCore/QCryptographicHash>

#include <nx/fusion/model_functions.h>

namespace {
/*!
This string represents client during search with NetworkOptixModuleFinder class.
It may look strange, but "client.exe" is valid on linux too (VER_ORIGINALFILENAME_STR from app_info.h)
*/
static const QString kNxClientId = lit("client.exe");
static const QString kNxECId = lit("Enterprise Controller");
static const QString kNxMediaServerId = lit("Media Server");

} // namespace

namespace nx {
namespace vms {
namespace api {

void ModuleInformation::fixRuntimeId()
{
    if (!runtimeId.isNull())
        return;

    constexpr int kBytesNeeded = 16;

    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(id.toRfc4122());
    md5.addData(customization.toLatin1());
    md5.addData(systemName.toLatin1());
    md5.addData(QByteArray::number(port));

    QByteArray hash = md5.result();
    while (hash.size() < kBytesNeeded)
        hash += hash;

    hash.resize(16);
    runtimeId = QnUuid::fromRfc4122(hash);
}

QString ModuleInformation::cloudId() const
{
    if (cloudSystemId.isEmpty())
        return QString();

    const auto tmpCloudSystemId = QnUuid::fromStringSafe(cloudSystemId);
    return id.toSimpleString() + "." +
        (tmpCloudSystemId.isNull() ? cloudSystemId : tmpCloudSystemId.toSimpleString());
}

QString ModuleInformation::nxMediaServerId()
{
    return kNxMediaServerId;
}

QString ModuleInformation::nxECId()
{
    return kNxECId;
}

QString ModuleInformation::nxClientId()
{
    return kNxClientId;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ModuleInformation)(ModuleInformationWithAddresses),
    (eq)(ubjson)(json)(xml)(csv_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
