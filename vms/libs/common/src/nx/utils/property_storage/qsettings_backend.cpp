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

QString QSettingsBackend::readValue(const QString& name, bool* success)
{
    if (success)
        *success = true;
    return m_settings->value(name).toString();
}

bool QSettingsBackend::writeValue(const QString& name, const QString& value)
{
    m_settings->setValue(name, value);
    return true;
}

bool QSettingsBackend::removeValue(const QString& name)
{
    m_settings->remove(name);
    return true;
}

bool QSettingsBackend::sync()
{
    m_settings->sync();
    return true;
}

} // namespace nx::utils::property_storage
