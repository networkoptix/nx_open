#pragma once

#include <QtCore/QScopedPointer>

#include "abstract_backend.h"

class QSettings;

namespace nx {
namespace utils {
namespace property_storage {

class QSettingsBackend: public AbstractBackend
{
public:
    QSettingsBackend(QSettings* settings, const QString& group = QString());
    virtual ~QSettingsBackend() override;

    virtual QString readValue(const QString& name) override;
    virtual void writeValue(const QString& name, const QString& value) override;
    virtual void removeValue(const QString& name) override;
    virtual void sync() override;

private:
    QScopedPointer<QSettings> m_settings;
};

} // namespace property_storage
} // namespace utils
} // namespace nx
