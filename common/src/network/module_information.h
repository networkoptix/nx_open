#pragma once

#include <QtCore/QString>
#include <QtCore/QSet>

#include <utils/common/software_version.h>
#include <utils/common/system_information.h>
#include <utils/common/id.h>
#include <nx/fusion/model_functions_fwd.h>

class QnConnectionInfo;

struct QnModuleInformation
{
    QString type;
    QString customization;
    QString brand;
    QnSoftwareVersion version;
    QnSystemInformation systemInformation;
    QString systemName;
    QString name;
    int port;
    QnUuid id;
    bool sslAllowed;
    int protoVersion;
    QnUuid runtimeId;
    Qn::ServerFlags serverFlags;
    QString realm;
    bool ecDbReadOnly;
    QString cloudSystemId;
    QString cloudPortalUrl;
    QString cloudHost;

    QnModuleInformation();

    void fixRuntimeId();
    QString cloudId() const;

    static QString nxMediaServerId();
    static QString nxECId();
    static QString nxClientId();

};

namespace helpers {
    /*
     * Extracts system id. Result is:
     * - identifier of server if it is in "new state"
     * - cloud id if server in cloud
     * - system name in other cases
     */
    QString getTargetSystemId(const QnConnectionInfo& info);
    QString getTargetSystemId(const QnModuleInformation& info);

} // helpers namespace

struct QnModuleInformationWithAddresses : QnModuleInformation
{
    QSet<QString> remoteAddresses;
    QnModuleInformationWithAddresses() {}
    QnModuleInformationWithAddresses(const QnModuleInformation &other) :
        QnModuleInformation(other)
    {}
};

#define QnModuleInformation_Fields (type)(customization)(version)(systemInformation) \
    (systemName)(name)(port)(id)(sslAllowed)(protoVersion)(runtimeId) \
    (serverFlags)(realm)(ecDbReadOnly)(cloudSystemId)(cloudHost)(brand)

#define QnModuleInformationWithAddresses_Fields QnModuleInformation_Fields(remoteAddresses)

QN_FUSION_DECLARE_FUNCTIONS(QnModuleInformation, (ubjson)(xml)(json)(metatype)(eq))

QN_FUSION_DECLARE_FUNCTIONS(QnModuleInformationWithAddresses, (ubjson)(xml)(json)(metatype)(eq))
