#pragma once

#include <QtCore/QString>

namespace nx::utils::property_storage {

class AbstractBackend
{
public:
    virtual ~AbstractBackend() = default;

    virtual QString readValue(const QString& name) = 0;
    virtual void writeValue(const QString& name, const QString& data) = 0;
    virtual void removeValue(const QString& name) = 0;
    virtual void sync() {}
};

} // namespace nx::utils::property_storage
