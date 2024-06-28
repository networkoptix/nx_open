// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "single_system_description.h"

namespace nx::vms::client::core {

class LocalSystemDescription;
using LocalSystemDescriptionPtr = QSharedPointer<LocalSystemDescription>;

class NX_VMS_CLIENT_CORE_API LocalSystemDescription: public SingleSystemDescription
{
    Q_OBJECT
    using base_type = SingleSystemDescription;

public:
    static LocalSystemDescriptionPtr createFactory(const QString& systemId);

    static LocalSystemDescriptionPtr create(
        const QString& systemId,
        const nx::Uuid& localSystemId,
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
    LocalSystemDescription(const QString& systemId);

    // Ctor for regular local system
    LocalSystemDescription(
        const QString& systemId,
        const nx::Uuid& localSystemId,
        const QString& systemName);

    void init();

    void updateNewSystemState();

private:
    bool m_isNewSystem;
};

} // namespace nx::vms::client::core
