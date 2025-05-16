// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "settings.h"

#include <nx/utils/deprecated_settings.h>

namespace nx::telemetry {

void Settings::load(const SettingsReader& settings, const QString& prefix)
{
    QnSettingsGroupReader group(settings, prefix);
    endpoint = group.value("endpoint").toString().toStdString();
}

} // namespace nx::telemetry
