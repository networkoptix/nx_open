#include "module_information.h"

#include <QtNetwork/QNetworkInterface>

#include <utils/common/model_functions.h>
#include <common/common_module.h>


bool QnModuleInformation::isCompatibleToCurrentSystem() const {
    return systemName == qnCommon->localSystemName() && isCompatible(version, qnCommon->engineVersion());
}

bool QnModuleInformation::isLocal() const {
    for (const QHostAddress &address: QNetworkInterface::allAddresses()) {
        if (remoteAddresses.contains(address.toString()))
            return true;
    }
    return false;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnModuleInformation, (json)(eq), QnModuleInformation_Fields)

