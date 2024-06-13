// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "generic_statistics_module.h"

#include <nx/reflect/to_string.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>

using namespace nx::vms::client::desktop;

namespace {

static const QString kLocaleTag("locale");

} // namespace

QnGenericStatisticsModule::QnGenericStatisticsModule(QObject* parent):
    base_type(parent)
{
}

QnStatisticValuesHash QnGenericStatisticsModule::values() const
{
    const auto settingsLocale = appContext()->coreSettings()->locale();
    const auto locale = settingsLocale.isEmpty() ? QLocale::system().name() : settingsLocale;
    const QString certificateValidation =
        QString::fromStdString(nx::reflect::enumeration::toString(
            appContext()->coreSettings()->certificateValidationLevel()));

    return {
        {kLocaleTag, locale},
        {"certificate_validation", certificateValidation},
    };
}

void QnGenericStatisticsModule::reset()
{
}
