// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>

#include <network/system_description.h>

class QnCloudSystemDescription: public QnSystemDescription
{
    Q_OBJECT
    using base_type = QnSystemDescription;

public:
    typedef QSharedPointer<QnCloudSystemDescription> PointerType;

    static PointerType create(
        const QString& systemId,
        const QnUuid& localSystemId,
        const QString& systemName,
        const QString& ownerEmail,
        const QString& ownerFullName,
        bool running);

    virtual ~QnCloudSystemDescription() = default;

    void setOnline(bool online);

    void updateLastKnownVersion(const nx::utils::SoftwareVersion& version);

public: // Overrides
    virtual bool isCloudSystem() const override;

    virtual bool isOnline() const override;

    virtual bool isNewSystem() const override;

    virtual QString ownerAccountEmail() const override;

    virtual QString ownerFullName() const override;

    virtual bool isOauthSupported() const override;

private:
    QnCloudSystemDescription(
        const QString& systemId,
        const QnUuid& localSystemId,
        const QString& systemName,
        const QString& ownerEmail,
        const QString& ownerFullName,
        bool online);

private:
    const QString m_ownerEmail;
    const QString m_ownerFullName;
    nx::utils::SoftwareVersion m_lastKnownVersion;

    bool m_online;
};
