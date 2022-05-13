// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/property_storage/abstract_backend.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API KeychainBackend: public nx::utils::property_storage::AbstractBackend
{
public:
    explicit KeychainBackend(const QString& serviceName);

    virtual QString readValue(const QString& name, bool* success = nullptr) override;
    virtual bool writeValue(const QString& name, const QString& value) override;
    virtual bool removeValue(const QString& name) override;

private:
    QString m_serviceName;
};

} // namespace nx::vms::client::core
