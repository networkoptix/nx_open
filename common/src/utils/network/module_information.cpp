#include "module_information.h"

#include <QtNetwork/QNetworkInterface>

#include <utils/common/model_functions.h>
#include <common/common_module.h>
#include <nx_ec/ec_proto_version.h>


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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnModuleInformation, (json)(eq), QnModuleInformation_Fields)

