#pragma once

#include "data.h"
#include "software_version.h"
#include "system_information.h"

#include <set>

#include <QtCore/QString>
#include <QtCore/QSet>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/resource_types.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API ModuleInformation: Data
{
    QString type;
    QString customization;
    QString brand;
    SoftwareVersion version;
    SystemInformation systemInformation;
    QString systemName;
    QString name;
    int port = 0;
    QnUuid id;
    bool sslAllowed = false;
    int protoVersion = 0;
    QnUuid runtimeId;
    ServerFlags serverFlags = {};
    QString realm;
    bool ecDbReadOnly = false;
    QString cloudSystemId;
    QString cloudPortalUrl;
    QString cloudHost;
    QnUuid localSystemId;

    void fixRuntimeId();
    QString cloudId() const;

    static QString nxMediaServerId();
    static QString nxECId();
    static QString nxClientId();
};

struct NX_VMS_API ModuleInformationWithAddresses: ModuleInformation
{
    QSet<QString> remoteAddresses;

    ModuleInformationWithAddresses() = default;
    ModuleInformationWithAddresses(const ModuleInformation& other): ModuleInformation(other) {}

    template<typename SocketAddress>
    std::set<SocketAddress> endpoints() const
    {
        std::set<SocketAddress> endpoints;
        for (const auto& address: remoteAddresses)
        {
            SocketAddress endpoint(address);
            if (endpoint.port == 0)
                endpoint.port = (decltype(endpoint.port)) port;

            endpoints.insert(std::move(endpoint));
        }

        return endpoints;
    }

    template<typename SocketAddressList>
    void setEndpoints(const SocketAddressList& endpoints)
    {
        remoteAddresses.clear();
        for (const auto& endpoint: endpoints)
        {
            if (endpoint.port == port)
                remoteAddresses.insert(endpoint.address.toString());
            else
                remoteAddresses.insert(endpoint.toString());
        }
    }
};

#define ModuleInformation_Fields \
    (type) \
    (customization) \
    (version) \
    (systemInformation) \
    (systemName) \
    (name) \
    (port) \
    (id) \
    (sslAllowed) \
    (protoVersion) \
    (runtimeId) \
    (serverFlags) \
    (realm) \
    (ecDbReadOnly) \
    (cloudSystemId) \
    (cloudHost) \
    (brand) \
    (localSystemId)

#define ModuleInformationWithAddresses_Fields \
    ModuleInformation_Fields \
    (remoteAddresses)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::ModuleInformation)
Q_DECLARE_METATYPE(nx::vms::api::ModuleInformationWithAddresses)
