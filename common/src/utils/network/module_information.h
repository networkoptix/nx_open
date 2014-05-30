#ifndef MODULE_INFORMATION_H
#define MODULE_INFORMATION_H

#include <QtCore/QString>
#include <QtCore/QSet>

#include <utils/common/software_version.h>
#include <utils/common/system_information.h>
#include <utils/common/id.h>
#include <utils/common/model_functions_fwd.h>

struct QnModuleInformation {
    QString type;
    QnSoftwareVersion version;
    QnSystemInformation systemInformation;
    QString systemName;
    quint16 port;
    QSet<QString> remoteAddresses;
    bool isLocal; //!< true if at least one address from \a remoteHostAddress is a local address
    QnId id;

    QnModuleInformation() : isLocal(false) {}
};
#define QnModuleInformation_Fields (type)(version)(systemInformation)(systemName)(port)(remoteAddresses)(isLocal)(id)

QN_FUSION_DECLARE_FUNCTIONS(QnModuleInformation, (json))

#endif // MODULE_INFORMATION_H
