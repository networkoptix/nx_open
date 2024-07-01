// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/log/format.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/module_information.h>

#include "system_description_fwd.h"

namespace nx::vms::client::core {

enum class QnServerField
{
    NoField = 0,
    Name = 1 << 0,
    SystemName = 1 << 1,
    Host = 1 << 2,
    CloudId = 1 << 3,
    Version = 1 << 4,
};
Q_DECLARE_FLAGS(QnServerFields, QnServerField)

enum class QnSystemCompatibility
{
    compatible,
    requireCompatibilityMode,
    incompatible
};

class NX_VMS_CLIENT_CORE_API SystemDescription: public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /**
     * Some implementation-defined id-ish string. See `SystemFinder` class documentation for better
     * understanding and actual implementation details.
     */
    virtual QString id() const = 0;

    /**
     * Local ID of the system. Can be empty (for the Factory systems), and generally speaking is
     * not guaranteed to be unique between different systems (as long as they have no physical
     * connectivity between each other).
     */
    virtual Uuid localId() const = 0;

    /**
     * Cloud ID of the system. Assigned when it is bound to the cloud, cleared when unbound.
     * Considered to be unique for all systems which are bound to the cloud.
     */
    virtual QString cloudId() const = 0;

    virtual QString name() const = 0;

    virtual QString ownerAccountEmail() const = 0;

    virtual QString ownerFullName() const = 0;

    virtual bool isCloudSystem() const = 0;

    virtual bool is2FaEnabled() const = 0;

    virtual bool isNewSystem() const = 0;

    virtual api::SaasState saasState() const = 0;

    using ServersList = QList<api::ModuleInformationWithAddresses>;
    virtual ServersList servers() const = 0;

    virtual bool containsServer(const Uuid& serverId) const = 0;

    virtual api::ModuleInformationWithAddresses getServer(const Uuid& serverId) const = 0;

    virtual bool isReachableServer(const Uuid& serverId) const = 0;

    // TODO: #ynikitenkov Rename host "field" to appropriate
    virtual nx::utils::Url getServerHost(const Uuid& serverId) const = 0;

    virtual QSet<nx::utils::Url> getServerRemoteAddresses(const Uuid& serverId) const = 0;

    virtual qint64 getServerLastUpdatedMs(const Uuid& serverId) const = 0;

    /**
     * We can successfully establish network connection with this system from the current machine.
     * Implies that the system is online.
     */
    virtual bool isReachable() const = 0;

    /**
     * We definitely know system is online - either because we successfully pinged it - or because
     * our cloud marked it as online.
     */
    virtual bool isOnline() const = 0;

    virtual bool hasLocalServer() const = 0;

    virtual QnSystemCompatibility systemCompatibility() const = 0;

    virtual bool isOauthSupported() const = 0;

    virtual nx::utils::SoftwareVersion version() const = 0;

    virtual QString idForToStringFromPtr() const = 0;

signals:
    void serverAdded(const Uuid& serverId);
    void serverChanged(const Uuid& serverId, QnServerFields flags);
    void serverRemoved(const Uuid& serverId);

    void isCloudSystemChanged();
    void system2faEnabledChanged();
    void ownerChanged();
    void systemNameChanged();
    void onlineStateChanged();
    void reachableStateChanged();
    void newSystemStateChanged();
    void oauthSupportedChanged();
    void versionChanged();
    void saasStateChanged();
    void organizationChanged();
};

} // namespace nx::vms::client::core
