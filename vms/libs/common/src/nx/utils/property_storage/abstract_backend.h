#pragma once

#include <QtCore/QString>

namespace nx::utils::property_storage {

class AbstractBackend
{
public:
    virtual ~AbstractBackend() = default;

    virtual QString readValue(const QString& name, bool* success = nullptr) = 0;
    virtual bool writeValue(const QString& name, const QString& data) = 0;
    virtual bool removeValue(const QString& name) = 0;
    virtual bool sync() { return false; }
};

} // namespace nx::utils::property_storage
