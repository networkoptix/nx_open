// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <network/cloud_system_data.h>

#include "single_system_description.h"

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API QnCloudSystemDescription: public SingleSystemDescription
{
    Q_OBJECT
    using base_type = SingleSystemDescription;

public:
    static QSharedPointer<QnCloudSystemDescription> create(const QnCloudSystem& system);

    virtual ~QnCloudSystemDescription() = default;

    void setOnline(bool online);

    void set2faEnabled(bool system2faEnabled);

    void setOrganization(const QString& organizationId);

    void updateLastKnownVersion(const nx::utils::SoftwareVersion& version);

public: // Overrides
    virtual bool isCloudSystem() const override;

    virtual bool is2FaEnabled() const override;

    virtual bool isOnline() const override;

    virtual bool isNewSystem() const override;

    virtual QString ownerAccountEmail() const override;

    virtual QString ownerFullName() const override;

    virtual bool isOauthSupported() const override;

    /** The VMS version reported by the last connected VMS server. */
    virtual nx::utils::SoftwareVersion version() const override;

private:
    QnCloudSystemDescription(
        const QString& systemId,
        const nx::Uuid& localSystemId,
        const QString& systemName,
        const QString& ownerEmail,
        const QString& ownerFullName,
        bool online,
        bool system2faEnabled,
        const QString& organizationId);

private:
    const QString m_ownerEmail;
    const QString m_ownerFullName;
    nx::utils::SoftwareVersion m_lastKnownVersion;

    bool m_online;
    bool m_system2faEnabled = false;
    QString m_organizationId;
};

using QnCloudSystemDescriptionPtr = QSharedPointer<QnCloudSystemDescription>;

} // namespace nx::vms::client::core
