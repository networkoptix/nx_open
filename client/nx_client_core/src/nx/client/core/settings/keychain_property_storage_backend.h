#pragma once

#include <nx/utils/property_storage/abstract_backend.h>

namespace nx::client::core {

class KeychainBackend: public nx::utils::property_storage::AbstractBackend
{
public:
    KeychainBackend(const QString& serviceName);

    virtual QString readValue(const QString& name) override;
    virtual void writeValue(const QString& name, const QString& value) override;
    virtual void removeValue(const QString& name) override;

private:
    QString m_serviceName;
};

} // namespace nx::client::core
