#include "module_information.h"

#include <QtNetwork/QNetworkInterface>

#include <utils/common/model_functions.h>
#include <common/common_module.h>
#include <nx_ec/ec_proto_version.h>

namespace {
    /*!
    This string represents client during search with NetworkOptixModuleFinder class.
    It may look strange, but "client.exe" is valid on linux too (VER_ORIGINALFILENAME_STR from app_info.h)
    */
    const QString nxClientId = lit("client.exe");
    const QString nxECId = lit("Enterprise Controller");
    const QString nxMediaServerId = lit("Media Server");
}

bool QnModuleInformation::isCompatibleToCurrentSystem() const {
    return hasCompatibleVersion() && systemName == qnCommon->localSystemName();
}

bool QnModuleInformation::hasCompatibleVersion() const {
    return  protoVersion == nx_ec::EC2_PROTO_VERSION &&
            isCompatible(version, qnCommon->engineVersion());
}

void QnModuleInformation::fixRuntimeId()
{
    if (!runtimeId.isNull())
        return;
    const int bytesNeeded = 16;

    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(id.toRfc4122());
    md5.addData(customization.toLatin1());
    md5.addData(systemName.toLatin1());
    md5.addData(QByteArray::number(port));

    QByteArray hash = md5.result();
    while (hash.size() < bytesNeeded)
        hash += hash;
    hash.resize(16);
    runtimeId = QnUuid::fromRfc4122(hash);
}

QString QnModuleInformation::nxMediaServerId() {
    return ::nxMediaServerId;
}

QString QnModuleInformation::nxECId() {
    return ::nxECId;
}

QString QnModuleInformation::nxClientId() {
    return ::nxClientId;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnModuleInformation, (ubjson)(xml)(json)(eq), QnModuleInformation_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnModuleInformationWithAddresses, (ubjson)(xml)(json)(eq), QnModuleInformationWithAddresses_Fields)
