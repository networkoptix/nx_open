// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "module_information.h"

#include <QtCore/QCryptographicHash>
#include <QtNetwork/QNetworkInterface>

#include <nx/fusion/model_functions.h>

namespace {

static const QString kMediaServerId = "Media Server";

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
    runtimeId = nx::Uuid::fromRfc4122(hash);
}

QString ModuleInformation::cloudId() const
{
    if (cloudSystemId.isEmpty())
        return QString();

    const auto tmpCloudSystemId = nx::Uuid::fromStringSafe(cloudSystemId);
    return id.toSimpleString() + "." +
        (tmpCloudSystemId.isNull() ? cloudSystemId : tmpCloudSystemId.toSimpleString());
}

bool ModuleInformation::isNewSystem() const
{
    return localSystemId.isNull();
}

bool ModuleInformation::isSaasSystem() const
{
    return organizationId && saasState != nx::vms::api::SaasState::uninitialized;
}

QString ModuleInformation::mediaServerId()
{
    return kMediaServerId;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ModuleInformation, (ubjson)(json)(xml)(csv_record), ModuleInformation_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ModuleInformationWithAddresses,
    (ubjson)(json)(xml)(csv_record), ModuleInformationWithAddresses_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TransactionLogTime, (json), TransactionLogTime_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ServerInformation, (json), ServerInformation_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ServerRuntimeInformation, (json), ServerRuntimeInformation_Fields)

} // namespace api
} // namespace vms
} // namespace nx

namespace nx::utils {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(OsInfo,
    (ubjson)(json)(xml)(csv_record),
    (platform)(variant)(variantVersion))

} // namespace nx::utils
