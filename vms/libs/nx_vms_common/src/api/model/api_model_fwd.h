// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>

namespace nx::vms::api {

struct StorageScanInfo;

struct StorageSpaceData;

typedef QList<nx::vms::api::StorageSpaceData> StorageSpaceDataList;

} // namespace nx::vms::api

typedef QList<nx::vms::api::StorageSpaceData> QnStorageSpaceDataList;

struct QnStorageSpaceReply;
struct QnVirtualCameraReply;
struct QnVirtualCameraStatusReply;

struct QnBackupStatusData;
