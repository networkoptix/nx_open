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

void QnModuleInformationEx::fixRuntimeId()
{
    if (!runtimeId.isNull())
        return;
    const int bytesNeeded = 16;

    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(id.toRfc4122());
    md5.addData(customization.toLatin1());
    md5.addData(systemName.toLatin1());
    md5.addData(QByteArray::number(port));
    for (const QString &address: remoteAddresses)
        md5.addData(address.toLatin1());

    QByteArray hash = md5.result();
    while (hash.size() < bytesNeeded)
        hash += hash;
    hash.resize(16);
    runtimeId = QnUuid::fromRfc4122(hash);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnModuleInformation,   (json)(eq), QnModuleInformation_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnModuleInformationEx, (json)(eq), QnModuleInformationEx_Fields)

