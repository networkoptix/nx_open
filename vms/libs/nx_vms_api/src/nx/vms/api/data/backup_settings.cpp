// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_settings.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BackupSettings, (json), BackupSettings_Fields)

QByteArray BackupSettings::toString() const
{
    return QJson::serialized(this);
}

} // namespace nx::vms::api
