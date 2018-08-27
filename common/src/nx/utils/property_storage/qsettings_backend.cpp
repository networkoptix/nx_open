#include "qsettings_backend.h"

#include <QtCore/QSettings>

namespace nx::utils::property_storage {

QSettingsBackend::QSettingsBackend(QSettings* settings, const QString& group):
    m_settings(settings)
{
    if (!group.isEmpty())
        m_settings->beginGroup(group);
}

QSettingsBackend::~QSettingsBackend()
{
}

QString QSettingsBackend::readValue(const QString& name)
{
    return m_settings->value(name).toString();
}

void QSettingsBackend::writeValue(const QString& name, const QString& value)
{
    m_settings->setValue(name, value);
}

void QSettingsBackend::removeValue(const QString& name)
{
    m_settings->remove(name);
}

void QSettingsBackend::sync()
{
    m_settings->sync();
}

} // namespace nx::utils::property_storage
