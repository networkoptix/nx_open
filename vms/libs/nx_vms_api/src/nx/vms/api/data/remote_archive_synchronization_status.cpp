// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_archive_synchronization_status.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    RemoteArchiveSynchronizationStatus,
    (json)(ubjson),
    RemoteArchiveSynchronizationStatus_Fields)

} // namespace nx::vms::api
