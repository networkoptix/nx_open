// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/data_macros.h>
#include <nx/vms/api/types/resource_types.h>

namespace nx::vms::api {

struct NX_VMS_API BackupSettings
{
    /**%apidoc
     * %param id Server id.
     */

    nx::Uuid id;
    CameraBackupQuality quality = CameraBackupQuality::CameraBackupBoth;
    bool backupNewCameras = false;

    bool operator==(const BackupSettings& other) const = default;

    nx::Uuid getId() const { return id; }
};

#define BackupSettings_Fields \
    (id) \
    (quality) \
    (backupNewCameras)

NX_VMS_API_DECLARE_STRUCT_AND_LIST(BackupSettings)
NX_REFLECTION_INSTRUMENT(BackupSettings, BackupSettings_Fields)

} // namespace nx::vms::api
