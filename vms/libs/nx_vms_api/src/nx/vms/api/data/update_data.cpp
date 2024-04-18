// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "update_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ProductInfo, (json), ProductInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(UpdateInfoRequest, (json), UpdateInfoRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(UpdateVersions, (json), UpdateVersions_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(UpdatePackage, (json), UpdatePackage_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(UpdateInformation, (json), UpdateInformation_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(StartUpdateReply, (json), StartUpdateReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DownloadStatus, (json), DownloadStatus_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(UpdateStatusInfo, (json), UpdateStatusInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(InstallUpdateRequest, (json), InstallUpdateRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(FinishUpdateRequest, (json), FinishUpdateRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    PersistentUpdateStorageInfo, (json), PersistentUpdateStorageInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    PersistentUpdateStorageRequest, (json), PersistentUpdateStorageRequest_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UpdateUploadData, (ubjson)(xml)(json)(sql_record)(csv_record), UpdateUploadData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(UpdateUploadResponseData,
    (ubjson)(xml)(json)(sql_record)(csv_record), UpdateUploadResponseData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UpdateInstallData, (ubjson)(xml)(json)(sql_record)(csv_record), UpdateInstallData_Fields)

} // namespace nx::vms::api
