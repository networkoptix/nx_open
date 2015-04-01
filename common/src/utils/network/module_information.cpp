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

bool QnModuleInformation::isLocal() const {
    for (const QHostAddress &address: QNetworkInterface::allAddresses()) {
        if (remoteAddresses.contains(address.toString()))
            return true;
    }
    return false;
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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnModuleInformation, (json)(eq), QnModuleInformation_Fields)

