// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

namespace nx::vms::statistics {

struct NX_VMS_COMMON_API StreamStorageInfo
{
    std::optional<std::chrono::seconds> archiveSizeS;
    std::optional<qint64> archiveSizeMb;
    std::optional<qint64> archiveIndexRecordCount;

    auto operator<=>(const StreamStorageInfo& other) const = default;
};
#define StreamStorageInfo_Fields (archiveSizeS)(archiveSizeMb)(archiveIndexRecordCount)

struct NX_VMS_COMMON_API CountAndAverageSize
{
    std::optional<qint64> recordCount;
    std::optional<int> recordAverageSize;

    auto operator<=>(const CountAndAverageSize& other) const = default;
};
#define CountAndAverageSize_Fields (recordCount)(recordAverageSize)

struct NX_VMS_COMMON_API DeviceStorageInfo
{
    /** Timestamp of the last update */
    std::optional<std::chrono::milliseconds> lastUpdateTimeMs;
    std::vector<StreamStorageInfo> streamStorageInfos;
    std::optional<qint64> motionIndexRecordCount;
    CountAndAverageSize bookmarkStats;

    auto operator<=>(const DeviceStorageInfo& other) const = default;
};
#define DeviceStorageInfo_Fields (lastUpdateTimeMs)(streamStorageInfos) \
    (motionIndexRecordCount)(bookmarkStats)

#define DEVICE_STORAGE_INFO_FUNCTIONS (ubjson)(xml)(json)(csv_record)

NX_REFLECTION_INSTRUMENT(StreamStorageInfo, StreamStorageInfo_Fields)
NX_REFLECTION_INSTRUMENT(CountAndAverageSize, CountAndAverageSize_Fields)
NX_REFLECTION_INSTRUMENT(DeviceStorageInfo, DeviceStorageInfo_Fields)

QN_FUSION_DECLARE_FUNCTIONS(StreamStorageInfo, DEVICE_STORAGE_INFO_FUNCTIONS, NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(CountAndAverageSize, DEVICE_STORAGE_INFO_FUNCTIONS, NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(DeviceStorageInfo, DEVICE_STORAGE_INFO_FUNCTIONS, NX_VMS_COMMON_API)

} // namespace nx::vms::statistics
