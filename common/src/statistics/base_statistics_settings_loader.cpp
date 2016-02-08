
#include "base_statistics_settings_loader.h"
#include <utils/common/model_functions.h>

namespace
{
    enum
    {
        kDefaultLimit = 1
        , kDefaultStoreDays = 30
    };
}

QnStatisticsSettings::QnStatisticsSettings()
    : limit(kDefaultLimit)
    , storeDays(kDefaultStoreDays)
    , filters()
{}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnStatisticsSettings, (json)(ubjson)(xml)(csv_record)(eq), QnStatisticsSettings_Fields)

QnBaseStatisticsSettingsLoader::QnBaseStatisticsSettingsLoader(QObject *parent)
    : base_type(parent)
{

}

QnBaseStatisticsSettingsLoader::~QnBaseStatisticsSettingsLoader()
{
}

QnStatisticsSettings QnBaseStatisticsSettingsLoader::settings()
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual method called!");
    return QnStatisticsSettings();
}

bool QnBaseStatisticsSettingsLoader::settingsAvailable()
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual method called!");
    return false;
}
