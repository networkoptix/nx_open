#include "generic_statistics_module.h"

#include <client/client_settings.h>

namespace {

static const QString kLocaleTag("locale");

} // namespace

QnGenericStatisticsModule::QnGenericStatisticsModule(QObject* parent):
    base_type(parent)
{
}

QnStatisticValuesHash QnGenericStatisticsModule::values() const
{
    const auto settingsLocale = qnSettings->locale();
    const auto locale = settingsLocale.isEmpty() ? QLocale::system().name() : settingsLocale;
    return {{kLocaleTag, locale}};
}

void QnGenericStatisticsModule::reset()
{
}
