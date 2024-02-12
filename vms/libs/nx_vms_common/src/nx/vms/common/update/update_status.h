// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/update/update_check.h>

namespace nx::vms::common::update {

struct NX_VMS_COMMON_API PackageDownloadStatus
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Status,
        downloading,
        downloaded,
        corrupted
    )

    QString file;
    Status status = Status::downloading;
    qint64 size = 0;
    qint64 downloadedBytes = 0;
};

#define PackageDownloadStatus_Fields (file)(status)(size)(downloadedBytes)
QN_FUSION_DECLARE_FUNCTIONS(PackageDownloadStatus, (json), NX_VMS_COMMON_API)

struct NX_VMS_COMMON_API Status
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Code,
        idle,
        /** Server is loading. **/
        starting,
        /** Server is downloading an update package. */
        downloading,
        /** Server is verifying an update package after downloading has finished successfully. */
        preparing,
        /** Update package has been downloaded and verified. */
        readyToInstall,
        latestUpdateInstalled,
        offline,
        error
    )

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(ErrorCode,
        noError = 0,
        updatePackageNotFound,
        osVersionNotSupported,
        noFreeSpaceToDownload,
        noFreeSpaceToExtract,
        noFreeSpaceToInstall,
        downloadFailed,
        invalidUpdateContents,
        corruptedArchive,
        extractionError,
        internalDownloaderError,
        internalError,
        unknownError,
        verificationError,
        installationError
    )

    nx::Uuid serverId;
    Code code = Code::idle;
    ErrorCode errorCode = ErrorCode::noError;
    QString message;
    int progress = 0;
    std::vector<PackageDownloadStatus> downloads;
    QString mainUpdatePackage;

    Status() = default;
    Status(
        const nx::Uuid& serverId,
        Code code,
        ErrorCode errorCode = ErrorCode::noError,
        int progress = 0);

    bool suitableForRetrying() const;

    friend NX_VMS_COMMON_API uint qHash(nx::vms::common::update::Status::ErrorCode key, uint seed);
};

#define UpdateStatus_Fields \
    (serverId)(code)(errorCode)(progress)(message)(downloads)(mainUpdatePackage)

using StatusList = std::vector<Status>;

QN_FUSION_DECLARE_FUNCTIONS(Status, (json), NX_VMS_COMMON_API)

} // namespace nx::vms::common::update
