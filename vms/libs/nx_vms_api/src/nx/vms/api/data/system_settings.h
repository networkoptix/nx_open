// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "watermark_settings.h"

namespace nx::vms::api {

struct SystemSettings
{
    QString cloudAccountName;
    QString cloudSystemID;
    QString defaultExportVideoCodec;
    QString localSystemId;
    QString systemName;
    WatermarkSettings watermarkSettings;
    bool webSocketEnabled = false;
};

NX_REFLECTION_INSTRUMENT(SystemSettings, (cloudAccountName)(cloudSystemID)(defaultExportVideoCodec)
    (localSystemId)(systemName)(watermarkSettings)(webSocketEnabled));

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::SystemSettings)
