// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/url.h>
#include <nx/vms/api/data/module_information.h>

class QnUuid;
class QnBaseSystemDescription;
typedef QSharedPointer<QnBaseSystemDescription> QnSystemDescriptionPtr;

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

class NX_VMS_CLIENT_CORE_API QnBaseSystemDescription: public QObject
{
    Q_OBJECT
    typedef QObject base_type;

public:
    QnBaseSystemDescription() : base_type()
    {
    }

    virtual ~QnBaseSystemDescription() {}

    virtual QString id() const = 0;

    virtual QnUuid localId() const = 0;

    virtual QString name() const = 0;

    virtual QString ownerAccountEmail() const = 0;

    virtual QString ownerFullName() const = 0;

    virtual bool isCloudSystem() const = 0;

    virtual bool is2FaEnabled() const = 0;

    virtual bool isNewSystem() const = 0;

    typedef QList<nx::vms::api::ModuleInformationWithAddresses> ServersList;
    virtual ServersList servers() const = 0;

    virtual bool containsServer(const QnUuid &serverId) const = 0;

    virtual nx::vms::api::ModuleInformationWithAddresses getServer(const QnUuid& serverId) const = 0;

    virtual bool isReachableServer(const QnUuid& serverId) const = 0;

    // TODO: #ynikitenkov Rename host "field" to appropriate
    virtual nx::utils::Url getServerHost(const QnUuid& serverId) const = 0;

    virtual QSet<nx::utils::Url> getServerRemoteAddresses(const QnUuid& serverId) const = 0;

    virtual qint64 getServerLastUpdatedMs(const QnUuid& serverId) const = 0;

    /**
     * We can successfully establish network connection with this system from the current machine.
     * Implies that the system is online.
     */
    virtual bool isReachable() const = 0;

    /**
     * We definitely know system is online - either because we successfully pinged it - or because
     * our cloud server marked it as online.
     */
    virtual bool isOnline() const = 0;

    virtual bool hasLocalServer() const = 0;

    virtual QnSystemCompatibility systemCompatibility() const = 0;

    virtual bool isOauthSupported() const = 0;

    virtual nx::utils::SoftwareVersion version() const = 0;

signals:
    void serverAdded(const QnUuid& serverId);
    void serverChanged(const QnUuid& serverId, QnServerFields flags);
    void serverRemoved(const QnUuid& serverId);

    void isCloudSystemChanged();
    void system2faEnabledChanged();
    void ownerChanged();
    void systemNameChanged();
    void onlineStateChanged();
    void reachableStateChanged();
    void newSystemStateChanged();
    void oauthSupportedChanged();
    void versionChanged();
};
