// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <network/system_description.h>

class QnLocalSystemDescription;
using LocalSystemDescriptionPtr = QSharedPointer<QnLocalSystemDescription>;

class NX_VMS_CLIENT_CORE_API QnLocalSystemDescription: public QnSystemDescription
{
    Q_OBJECT
    using base_type = QnSystemDescription;

public:
    static LocalSystemDescriptionPtr createFactory(const QString& systemId);

    static LocalSystemDescriptionPtr create(
        const QString& systemId,
        const nx::Uuid& localSystemId,
        const QString& cloudSystemId,
        const QString& systemName);

public: // Overrides
    virtual bool isCloudSystem() const override;

    virtual bool is2FaEnabled() const override;

    virtual bool isOnline() const override;

    virtual bool isNewSystem() const override;

    virtual QString ownerAccountEmail() const override;

    virtual QString ownerFullName() const override;

private:
    // Ctor for factory system
    QnLocalSystemDescription(const QString& systemId);

    // Ctor for regular local system
    QnLocalSystemDescription(
        const QString& systemId,
        const nx::Uuid& localSystemId,
        const QString& cloudSystemId,
        const QString& systemName);

    void init();

    void updateNewSystemState();

private:
    bool m_isNewSystem;
};
