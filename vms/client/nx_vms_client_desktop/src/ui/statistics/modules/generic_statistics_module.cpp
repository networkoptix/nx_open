// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "generic_statistics_module.h"

#include <client/client_settings.h>
#include <nx/reflect/to_string.h>

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
    const QString certificateValidation =
        QString::fromStdString(
            nx::reflect::enumeration::toString(qnSettings->certificateValidationLevel()));

    return {
        {kLocaleTag, locale},
        {"certificate_validation", certificateValidation},
    };
}

void QnGenericStatisticsModule::reset()
{
}
