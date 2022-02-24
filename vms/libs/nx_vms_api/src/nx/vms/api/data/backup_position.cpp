// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_position.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

BackupPositionEx::BackupPositionEx(const BackupPosition& other)
{
    positionLowMs = other.positionLowMs;
    positionHighMs = other.positionHighMs;
    bookmarkStartPositionMs = other.bookmarkStartPositionMs;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    BackupPositionIdData,
    (json)(ubjson),
    BackupPositionIdData_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    BackupPosition,
    (json)(ubjson),
    BackupPosition_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    BackupPositionEx,
    (json)(ubjson),
    BackupPositionEx_Fields)

} // namespace nx::vms::api
