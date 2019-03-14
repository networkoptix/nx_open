#include "statistics_settings.h"

#include <nx/fusion/model_functions.h>

namespace {

static const int kDefaultLimit = 1;
static const int kDefaultStoreDays = 30;
static const int kDefaultMinSendPeriodSecs = 60 * 60;   // 1 hour

}

QnStatisticsSettings::QnStatisticsSettings():
    limit(kDefaultLimit),
    storeDays(kDefaultStoreDays),
    minSendPeriodSecs(kDefaultMinSendPeriodSecs)
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
(QnStatisticsSettings), (json)(ubjson)(xml)(csv_record)(eq), _Fields)
