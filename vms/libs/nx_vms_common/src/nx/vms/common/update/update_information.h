// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/update/update_check.h>

namespace nx::vms::common::update {

struct NX_VMS_COMMON_API Information: nx::vms::update::PublicationInfo
{
    QList<QnUuid> participants = {};
    std::chrono::milliseconds lastInstallationRequestTime{-1};

    bool isValid() const;
    bool isEmpty() const;
};

#define Information_Fields \
    PublicationInfo_Fields \
    (participants) \
    (lastInstallationRequestTime)

QN_FUSION_DECLARE_FUNCTIONS(Information, (json), NX_VMS_COMMON_API)

enum class InformationError
{
    noError,
    networkError,
    httpError,
    jsonError,
    emptyPackagesUrls,
    brokenPackageError,
    missingPackageError,
    incompatibleVersion,
    incompatibleCloudHost,
    notFoundError,
    /** Error when client tried to contact mediaserver for update verification. */
    serverConnectionError,
    /**
     * Error when there's an attempt to parse packages.json for a version < 4.0 or an attempt to
     * parse update.json for a version >= 4.0.
     */
    incompatibleParser,
    noNewVersion,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(InformationError*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<InformationError>;
    return visitor(
        Item{InformationError::noError, "no error"},
        Item{InformationError::networkError, "network error"},
        Item{InformationError::httpError, "http error"},
        Item{InformationError::jsonError, "json error"},
        Item{InformationError::brokenPackageError, "local update package is corrupted"},
        Item{InformationError::missingPackageError, "missing files in the update package"},
        Item{InformationError::incompatibleVersion, "incompatible version"},
        Item{InformationError::incompatibleCloudHost, "incompatible cloud host"},
        Item{InformationError::notFoundError, "not found"},
        Item{InformationError::serverConnectionError, "failed to get response from mediaserver"},
        Item{InformationError::noNewVersion, "no new version"}
    );
}

NX_VMS_COMMON_API InformationError fetchErrorToInformationError(
    const nx::vms::update::FetchError error);

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

    QnUuid serverId;
    Code code = Code::idle;
    ErrorCode errorCode = ErrorCode::noError;
    QString message;
    int progress = 0;

    Status() = default;
    Status(
        const QnUuid& serverId,
        Code code,
        ErrorCode errorCode = ErrorCode::noError,
        int progress = 0);

    bool suitableForRetrying() const;

    friend NX_VMS_COMMON_API uint qHash(nx::vms::common::update::Status::ErrorCode key, uint seed);
};

#define UpdateStatus_Fields (serverId)(code)(errorCode)(progress)(message)

using StatusList = std::vector<Status>;

QN_FUSION_DECLARE_FUNCTIONS(Status, (json), NX_VMS_COMMON_API)

struct NX_VMS_COMMON_API ClientPackageStatus
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Status,
        downloading,
        downloaded,
        error
    )

    QString file;
    Status status = Status::downloading;
    int progress = 0;
};
#define ClientPackageStatus_Fields (file)(status)(progress)
QN_FUSION_DECLARE_FUNCTIONS(ClientPackageStatus, (json), NX_VMS_COMMON_API)

struct NX_VMS_COMMON_API OverallClientPackageStatus
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Status,
        idle,
        downloading,
        downloaded,
        error
    )

    QnUuid serverId;
    Status status = Status::idle;
    std::vector<ClientPackageStatus> packages;
    int progress = 0;
};
#define OverallClientPackageStatus_Fields (serverId)(status)(packages)(progress)
QN_FUSION_DECLARE_FUNCTIONS(OverallClientPackageStatus, (json), NX_VMS_COMMON_API)

} // namespace nx::vms::common::update

Q_DECLARE_METATYPE(nx::vms::common::update::Information)
