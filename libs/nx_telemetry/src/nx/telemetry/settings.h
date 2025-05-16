// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <QtCore/QString>

#include <nx/reflect/instrument.h>

class SettingsReader;

namespace nx::telemetry {

class NX_TELEMETRY_API Settings
{
public:
    std::string endpoint;

    void load(const SettingsReader& settings, const QString& prefix = QStringLiteral("telemetry"));
};
NX_REFLECTION_INSTRUMENT(Settings, (endpoint))

} // namespace nx::telemetry
