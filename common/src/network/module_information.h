#pragma once

#include <set>
#include <QtCore/QString>
#include <QtCore/QSet>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/socket_common.h>
#include <nx/utils/uuid.h>

#include <common/common_globals.h>
#include <utils/common/software_version.h>
#include <utils/common/system_information.h>

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
    QnUuid localSystemId;

    QnModuleInformation();

    void fixRuntimeId();
    QString cloudId() const;

    static QString nxMediaServerId();
    static QString nxECId();
    static QString nxClientId();
};

struct QnModuleInformationWithAddresses : QnModuleInformation
{
    QSet<QString> remoteAddresses;
    QnModuleInformationWithAddresses() {}
    QnModuleInformationWithAddresses(const QnModuleInformation &other) :
        QnModuleInformation(other)
    {}

    // TODO: Also make a tempate.
    inline
    std::set<nx::network::SocketAddress> endpoints() const
    {
        std::set<nx::network::SocketAddress> endpoints;
        for (const auto& address: remoteAddresses)
        {
            nx::network::SocketAddress endpoint(address);
            if (endpoint.port == 0)
                endpoint.port = (uint16_t) port;

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
            if (endpoint.port == (uint16_t) port)
                remoteAddresses.insert(endpoint.address.toString());
            else
                remoteAddresses.insert(endpoint.toString());
        }
    }
};

#define QnModuleInformation_Fields (type)(customization)(version)(systemInformation) \
    (systemName)(name)(port)(id)(sslAllowed)(protoVersion)(runtimeId) \
    (serverFlags)(realm)(ecDbReadOnly)(cloudSystemId)(cloudHost)(brand)(localSystemId)

#define QnModuleInformationWithAddresses_Fields QnModuleInformation_Fields(remoteAddresses)

QN_FUSION_DECLARE_FUNCTIONS(QnModuleInformation, (ubjson)(xml)(json)(metatype)(eq))

QN_FUSION_DECLARE_FUNCTIONS(QnModuleInformationWithAddresses, (ubjson)(xml)(json)(metatype)(eq))
