
#include "abstract_statistics_settings_loader.h"
#include <nx/fusion/model_functions.h>

namespace
{
    enum
    {
        kDefaultLimit = 1
        , kDefaultStoreDays = 30
        , kDefaultMinSendPeriodSecs = 60 * 60   // 1 hour
    };
}

QnStatisticsSettings::QnStatisticsSettings()
    : limit(kDefaultLimit)
    , storeDays(kDefaultStoreDays)
    , minSendPeriodSecs(kDefaultMinSendPeriodSecs)
    , filters()
{}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnStatisticsSettings), (json)(ubjson)(xml)(csv_record)(eq), _Fields)

QnAbstractStatisticsSettingsLoader::QnAbstractStatisticsSettingsLoader(QObject *parent)
    : base_type(parent)
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
