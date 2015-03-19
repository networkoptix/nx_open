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
    int port;
    QnUuid id;
    bool sslAllowed;
    QByteArray authHash;
    int protoVersion;
    QnUuid runtimeId;

    QnModuleInformation() : port(0), sslAllowed(false), protoVersion(0) {}

    bool isCompatibleToCurrentSystem() const;
    bool hasCompatibleVersion() const;
    void fixRuntimeId();
};

struct QnModuleInformationWithAddresses : QnModuleInformation {
    QSet<QString> remoteAddresses;
    QnModuleInformationWithAddresses() {}
    QnModuleInformationWithAddresses(const QnModuleInformation &other) :
        QnModuleInformation(other)
    {}
};

#define QnModuleInformation_Fields (type)(customization)(version)(systemInformation)(systemName)(name)(port)(id)(sslAllowed)(authHash)(protoVersion)(runtimeId)
#define QnModuleInformationWithAddresses_Fields QnModuleInformation_Fields(remoteAddresses)

QN_FUSION_DECLARE_FUNCTIONS(QnModuleInformation, (ubjson)(xml)(json)(metatype)(eq))

QN_FUSION_DECLARE_FUNCTIONS(QnModuleInformationWithAddresses, (xml)(json)(metatype)(eq))

#endif // MODULE_INFORMATION_H
