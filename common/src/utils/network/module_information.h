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
    QString name;
    quint16 port;
    QSet<QString> remoteAddresses;
    QnUuid id;
    bool sslAllowed;
    QByteArray authHash;
    int protoVersion;

    QnModuleInformation() : port(0), sslAllowed(false), protoVersion(0) {}

    bool isCompatibleToCurrentSystem(bool ignoreProtoVersion = false) const;
    bool isLocal() const; //!< true if at least one address from \a remoteHostAddress is a local address
};
#define QnModuleInformation_Fields (type)(customization)(version)(systemInformation)(systemName)(name)(port)(remoteAddresses)(id)(sslAllowed)(authHash)(protoVersion)

QN_FUSION_DECLARE_FUNCTIONS(QnModuleInformation, (json)(metatype)(eq))

#endif // MODULE_INFORMATION_H
