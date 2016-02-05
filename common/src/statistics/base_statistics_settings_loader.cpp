
#include "base_statistics_settings_loader.h"

QnBaseStatisticsSettingsLoader::QnBaseStatisticsSettingsLoader(QObject *parent)
    : base_type(parent)
{

}

QnBaseStatisticsSettingsLoader::~QnBaseStatisticsSettingsLoader()
{

}

QnStatisticsSettings QnBaseStatisticsSettingsLoader::settings() const
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual method called!");
    return QnStatisticsSettings();
}
