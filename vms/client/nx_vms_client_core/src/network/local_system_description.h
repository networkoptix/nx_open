// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <network/system_description.h>

class QnLocalSystemDescription: public QnSystemDescription
{
    Q_OBJECT
    using base_type = QnSystemDescription;

public:
    static PointerType createFactory(const QString& systemId);

    static PointerType create(
        const QString& systemId,
        const QnUuid& localSystemId,
        const QString& systemName);

public: // Overrides
    virtual bool isCloudSystem() const override;

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
        const QnUuid& localSystemId,
        const QString& systemName);

    virtual ~QnLocalSystemDescription() = default;

    void init();

    void updateNewSystemState();

private:
    bool m_isNewSystem;
};
