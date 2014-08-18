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
    QString customization;
    QnSoftwareVersion version;
    QnSystemInformation systemInformation;
    QString systemName;
    quint16 port;
    QSet<QString> remoteAddresses;
    bool isLocal; //!< true if at least one address from \a remoteHostAddress is a local address
    QUuid id;

    QnModuleInformation() : port(0), isLocal(false) {}
};
#define QnModuleInformation_Fields (type)(customization)(version)(systemInformation)(systemName)(port)(remoteAddresses)(isLocal)(id)

QN_FUSION_DECLARE_FUNCTIONS(QnModuleInformation, (json)(metatype))

#endif // MODULE_INFORMATION_H
