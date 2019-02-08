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
    /**
     * Note: backend takes ownership of QSettings object.
     */
    QSettingsBackend(QSettings* settings, const QString& group = QString());
    virtual ~QSettingsBackend() override;

    virtual QString readValue(const QString& name, bool* success = nullptr) override;
    virtual bool writeValue(const QString& name, const QString& value) override;
    virtual bool removeValue(const QString& name) override;
    virtual bool sync() override;

private:
    QScopedPointer<QSettings> m_settings;
};

} // namespace property_storage
} // namespace utils
} // namespace nx
