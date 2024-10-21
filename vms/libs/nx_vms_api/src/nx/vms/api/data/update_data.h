// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include <nx/utils/software_version.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>

#include "data_macros.h"
#include "persistent_update_storage.h"

namespace nx::vms::api {

NX_REFLECTION_ENUM(UpdateAction,
    /**%apidoc Starts update process. */
    start,

    /**%apidoc Initiates update package installation. */
    install,

    /**%apidoc Retries the latest failed update action. */
    retry,

    /**%apidoc Completes the update process. */
    finish
)

NX_REFLECTION_ENUM_CLASS(UpdateComponent,
    /**%apidoc[unused] Component is not known. */
    unknown,

    /**%apidoc Desktop Client. */
    client,

    /**%apidoc Desktop Client with custom branding. */
    customClient,

    /**%apidoc VMS Server. */
    server
)

NX_REFLECTION_ENUM(InformationCategory,
    /**%apidoc The version to be installed. */
    target,

    /**%apidoc The currently installed version. */
    installed,

    /**%apidoc The latest available version. */
    latest,

    /**%apidoc The explicitly specified version. */
    specific
)

NX_REFLECTION_ENUM_CLASS(PublicationType,
    /**%apidoc Local developer build. */
    local,

    /**%apidoc Private build for QA team. */
    private_build,

    /**%apidoc Private patch for a single setup. */
    private_patch,

    /**%apidoc Regular monthly patch. */
    patch,

    /**%apidoc Public beta version. */
    beta,

    /**%apidoc Public release candidate. */
    rc,

    /**%apidoc Public release. */
    release
)

struct NX_VMS_API ProductInfo
{
    /**%apidoc[opt] */
    UpdateComponent updateComponent{UpdateComponent::server};
    /**%apidoc[opt] */
    PublicationType publicationType{PublicationType::release};
    /**%apidoc[opt]
     * If present, the Server makes an attempt to retrieve an update manifest for the specified
     * version id from the dedicated updates server and return it as a result.
     */
    std::string version{};

    /**%apidoc[opt] Integer version of the release
     * %example 50100
     */
    int protocolVersion{0};
};
#define ProductInfo_Fields (updateComponent)(publicationType)(version)(protocolVersion)
NX_VMS_API_DECLARE_STRUCT_EX(ProductInfo, (json))
NX_REFLECTION_INSTRUMENT(ProductInfo, ProductInfo_Fields)

struct NX_VMS_API UpdateInfoRequest: ProductInfo
{
    /**%apidoc[opt] Information Category to request. */
    InformationCategory infoCategory{target};
};
#define UpdateInfoRequest_Fields ProductInfo_Fields (infoCategory)
NX_VMS_API_DECLARE_STRUCT_EX(UpdateInfoRequest, (json))
NX_REFLECTION_INSTRUMENT(UpdateInfoRequest, UpdateInfoRequest_Fields)

struct NX_VMS_API UpdateVersions
{
    std::string minVersion;
    std::string maxVersion;
};
#define UpdateVersions_Fields (minVersion)(maxVersion)
NX_VMS_API_DECLARE_STRUCT_EX(UpdateVersions, (json))
NX_REFLECTION_INSTRUMENT(UpdateVersions, UpdateVersions_Fields)

struct NX_VMS_API UpdatePackage
{
    std::string platform;
    std::optional<std::string> customClientVariant;
    std::map<std::string, UpdateVersions> platformVariants;

    std::string file;
    QString md5;

    /**%apidoc:integer Size of update package in bytes.
     * %example 1073741824
     */
    double sizeB{0.0};
    std::optional<std::string> signature;

    std::string url;
};
#define UpdatePackage_Fields (platform)(customClientVariant)(platformVariants)(file)(md5)(sizeB) \
    (signature)(url)

NX_VMS_API_DECLARE_STRUCT_EX(UpdatePackage, (json))
NX_REFLECTION_INSTRUMENT(UpdatePackage, UpdatePackage_Fields)

struct NX_VMS_API UpdateInformation
{
    std::string version;
    std::string cloudHost;
    std::string eulaLink;

    /**%apidoc Version of the latest read and accepted EULA.
     * %example 50100
     */
    int eulaVersion{0};
    std::string releaseNotesUrl;

    /**%apidoc Release date in milliseconds since epoch.
     * %example 86400000
     */
    std::chrono::milliseconds releaseDateMs{};

    /**%apidoc Maximum days for release delivery.
     * %example 90
     */
    int releaseDeliveryDays{0};
    std::string description;
    std::string eula;

    /**%apidoc Map of update components to a list of possible update packages. */
    std::map<UpdateComponent, std::vector<UpdatePackage>> packages;
    std::string url;

    /**%apidoc:integer Bytes of free space on server.
     * %example 1073741824
     */
    double freeSpaceB{0.0};
    std::vector<nx::Uuid> participants;

    /**%apidoc The time the Server was last updated in milliseconds since epoch.
     * %example 86400000
     */
    std::chrono::milliseconds lastInstallationRequestTimeMs{-1};
};
#define UpdateInformation_Fields (version)(cloudHost) (eulaLink)(eulaVersion)(releaseNotesUrl) \
    (releaseDateMs)(releaseDeliveryDays)(description)(eula)(packages)(url)(freeSpaceB) \
    (participants)(lastInstallationRequestTimeMs)
NX_VMS_API_DECLARE_STRUCT_EX(UpdateInformation, (json))
NX_REFLECTION_INSTRUMENT(UpdateInformation, UpdateInformation_Fields)

struct NX_VMS_API StartUpdateReply
{
    UpdateInformation updateInformation;
    std::vector<nx::Uuid> persistentStorageServers;
};
#define StartUpdateReply_Fields (updateInformation)(persistentStorageServers)
NX_VMS_API_DECLARE_STRUCT_EX(StartUpdateReply, (json))

struct NX_VMS_API InstallUpdateRequest
{
    std::vector<nx::Uuid> peers;
};
#define InstallUpdateRequest_Fields (peers)
NX_VMS_API_DECLARE_STRUCT_EX(InstallUpdateRequest, (json))
NX_REFLECTION_INSTRUMENT(InstallUpdateRequest, InstallUpdateRequest_Fields)

struct NX_VMS_API FinishUpdateRequest
{
    /**%apidoc[opt]
     * Force an update process completion regardless of actual peer state.
     * %example false
     */
    bool ignorePendingPeers{false};
};
#define FinishUpdateRequest_Fields (ignorePendingPeers)
NX_VMS_API_DECLARE_STRUCT_EX(FinishUpdateRequest, (json))
NX_REFLECTION_INSTRUMENT(FinishUpdateRequest, FinishUpdateRequest_Fields)

struct NX_VMS_API DownloadStatus
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Status,
        downloading,
        downloaded,
        corrupted)

    std::string file;
    Status status{Status::downloading};

    /**%apidoc:integer Size of file to download in bytes.
     * %example 1073741824
     */
    double sizeB{0.0};

    /**%apidoc:integer Number of bytes currently downloaded.
     * %example 536870912
     */
    double downloadedSizeB{0.0};
};
#define DownloadStatus_Fields (file)(status)(sizeB)(downloadedSizeB)
NX_VMS_API_DECLARE_STRUCT_EX(DownloadStatus, (json))

struct NX_VMS_API UpdateStatusInfo
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(State,
        idle,
        starting,
        downloading,
        preparing,
        readyToInstall,
        latestUpdateInstalled,
        offline,
        error)

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Error,
        noError,
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
        installationError)

    nx::Uuid serverId;
    State state{State::idle};

    /**%apidoc Only significant if 'state' is 'error'
     */
    Error error{Error::noError};
    std::string message;
    int progress{0};
    std::vector<DownloadStatus> downloads;
    std::string mainUpdatePackage;
};
#define UpdateStatusInfo_Fields (serverId)(state)(error)(message)(progress)(downloads)\
    (mainUpdatePackage)
NX_VMS_API_DECLARE_STRUCT_EX(UpdateStatusInfo, (json))

struct NX_VMS_API PersistentUpdateStorageInfo: PersistentUpdateStorage
{
    InformationCategory infoCategory;
};
#define PersistentUpdateStorageInfo_Fields  PersistentUpdateStorage_Fields(infoCategory)
NX_VMS_API_DECLARE_STRUCT_EX(PersistentUpdateStorageInfo, (json))
NX_REFLECTION_INSTRUMENT(PersistentUpdateStorageInfo, PersistentUpdateStorageInfo_Fields)

struct NX_VMS_API PersistentUpdateStorageRequest
{
    InformationCategory infoCategory;
};
#define PersistentUpdateStorageRequest_Fields (infoCategory)
NX_VMS_API_DECLARE_STRUCT_EX(PersistentUpdateStorageRequest, (json))
NX_REFLECTION_INSTRUMENT(PersistentUpdateStorageRequest, PersistentUpdateStorageRequest_Fields)

struct NX_VMS_API UpdateUploadData
{
    QString updateId;
    QByteArray data;
    qint64 offset{0};
};
#define UpdateUploadData_Fields \
    (updateId) \
    (data) \
    (offset)
NX_VMS_API_DECLARE_STRUCT(UpdateUploadData)

struct NX_VMS_API UpdateUploadResponseData
{
    nx::Uuid id;
    QString updateId;
    int chunks{0};
};
#define UpdateUploadResponseData_Fields (id)(updateId)(chunks)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(UpdateUploadResponseData)

struct NX_VMS_API UpdateInstallData
{
    QString updateId;
};
#define UpdateInstallData_Fields (updateId)
NX_VMS_API_DECLARE_STRUCT(UpdateInstallData)

} // namespace nx::vms::api
