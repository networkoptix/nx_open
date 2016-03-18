
#include "core_settings.h"

#include <QtCore/QSettings>

namespace
{
    const auto kCoreSettingsFile = lit("core_client_settings");
}

QnCoreSettings::QnCoreSettings(QObject *parent)
    : m_settings(new QSettings(kCoreSettingsFile))
{
    init();
    updateFromSettings(m_settings.data());
}

QnCoreSettings::~QnCoreSettings()
{
    submitToSettings(m_settings.data());
}
