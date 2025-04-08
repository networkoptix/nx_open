// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qsettings_backend.h"

#include <QtCore/QSettings>

namespace nx::utils::property_storage {

QSettingsBackend::QSettingsBackend(QSettings* settings, const QString& group):
    m_settings(settings),
    m_group(group)
{
}

QSettingsBackend::~QSettingsBackend()
{
}

bool QSettingsBackend::isWritable() const
{
    return m_settings->isWritable();
}

QString QSettingsBackend::readValue(const QString& name, bool* success)
{
    if (success)
        *success = true;
    return m_settings->value(m_group + "/" + name).toString();
}

bool QSettingsBackend::writeValue(const QString& name, const QString& value)
{
    m_settings->setValue(m_group + "/" + name, value);
    return true;
}

bool QSettingsBackend::removeValue(const QString& name)
{
    m_settings->remove(m_group + "/" + name);
    return true;
}

bool QSettingsBackend::exists(const QString& name) const
{
    return m_settings->contains(m_group + "/" + name);
}

bool QSettingsBackend::sync()
{
    m_settings->sync();
    return true;
}

QSettings* QSettingsBackend::qSettings() const
{
    return m_settings.get();
}

} // namespace nx::utils::property_storage
