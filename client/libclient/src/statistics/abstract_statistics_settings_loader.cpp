#include "abstract_statistics_settings_loader.h"

QnAbstractStatisticsSettingsLoader::QnAbstractStatisticsSettingsLoader(QObject *parent):
    base_type(parent)
{
}

QnAbstractStatisticsSettingsLoader::~QnAbstractStatisticsSettingsLoader()
{
}

QnStatisticsSettings QnAbstractStatisticsSettingsLoader::settings()
{
    NX_ASSERT(false, Q_FUNC_INFO, "Pure virtual method called!");
    return QnStatisticsSettings();
}

bool QnAbstractStatisticsSettingsLoader::settingsAvailable()
{
    NX_ASSERT(false, Q_FUNC_INFO, "Pure virtual method called!");
    return false;
}
