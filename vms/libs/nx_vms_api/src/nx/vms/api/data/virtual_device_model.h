// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/rest/result.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

struct NX_VMS_API FlexibleId
{
    /**%apidoc
     * Virtual Device id, can be obtained from "id" field via
     * <code>GET /rest/v{3-}/devices/&ast;/virtual</code>.
     */
    QString id;
};
#define FlexibleId_Fields (id)
QN_FUSION_DECLARE_FUNCTIONS(FlexibleId, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(FlexibleId, FlexibleId_Fields);

struct NX_VMS_API VirtualDeviceCreate
{
    /**%apidoc Name of the virtual Device to be created. */
    QString name;
};
#define VirtualDeviceCreate_Fields (name)
QN_FUSION_DECLARE_FUNCTIONS(VirtualDeviceCreate, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(VirtualDeviceCreate, VirtualDeviceCreate_Fields);

struct NX_VMS_API VirtualDeviceLockInfo
{
    /**%apidoc User who owns the lock. */
    nx::Uuid userId;

    /**%apidoc Lock token. */
    nx::Uuid token;

    /**%apidoc Lock timeout in milliseconds. */
    std::chrono::milliseconds ttlMs;

    /**%apidoc Consume progress, if ongoing. */
    int progress = 0;
};
#define VirtualDeviceLockInfo_Fields (userId)(token)(ttlMs)(progress)
QN_FUSION_DECLARE_FUNCTIONS(VirtualDeviceLockInfo, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(VirtualDeviceLockInfo, VirtualDeviceLockInfo_Fields);

struct NX_VMS_API VirtualDeviceStatus: FlexibleId
{
    /**%apidoc Lock state information, if the Device is locked. */
    std::optional<VirtualDeviceLockInfo> lockInfo;
};
#define VirtualDeviceStatus_Fields FlexibleId_Fields(lockInfo)
QN_FUSION_DECLARE_FUNCTIONS(VirtualDeviceStatus, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(VirtualDeviceStatus, VirtualDeviceStatus_Fields);

struct NX_VMS_API VirtualDeviceFootageElement
{
    /**%apidoc:integer Footage element size in Bytes.
     * %example 4096
     */
    double sizeB = 0;

    /**%apidoc Start time in milliseconds.
     * %example 0
     */
    std::chrono::milliseconds startTimeMs{0};

    /**%apidoc
     * Duration in milliseconds.
     * %example 60000
     */
    std::chrono::milliseconds durationMs{0};
};
#define VirtualDeviceFootageElement_Fields (sizeB)(startTimeMs)(durationMs)
QN_FUSION_DECLARE_FUNCTIONS(VirtualDeviceFootageElement, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(VirtualDeviceFootageElement, VirtualDeviceFootageElement_Fields);

struct NX_VMS_API VirtualDevicePrepare: FlexibleId
{
    /**%apidoc[opt] Footage to upload. */
    std::vector<VirtualDeviceFootageElement> footage;
};
#define VirtualDevicePrepare_Fields FlexibleId_Fields(footage)
QN_FUSION_DECLARE_FUNCTIONS(VirtualDevicePrepare, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(VirtualDevicePrepare, VirtualDevicePrepare_Fields);

struct VirtualDevicePrepareReply
{
    std::vector<VirtualDeviceFootageElement> footage;
    bool storageCleanupNeeded = false;
    bool storageFull = false;
};
#define VirtualDevicePrepareReply_Fields (footage)(storageCleanupNeeded)(storageFull)
QN_FUSION_DECLARE_FUNCTIONS(VirtualDevicePrepareReply, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(VirtualDevicePrepareReply, VirtualDevicePrepareReply_Fields);

struct NX_VMS_API VirtualDeviceConsume: FlexibleId
{
    /**%apidoc Token acquired when the virtual Device was locked. */
    nx::Uuid token;

    /**%apidoc Name of the previously uploaded file. */
    QString uploadId;

    /**%apidoc Starting time of the file in milliseconds since epoch.
     * %example 0
     */
    std::chrono::milliseconds startTimeMs;
};
#define VirtualDeviceConsume_Fields FlexibleId_Fields(token)(uploadId)(startTimeMs)
QN_FUSION_DECLARE_FUNCTIONS(VirtualDeviceConsume, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(VirtualDeviceConsume, VirtualDeviceConsume_Fields);

// TODO: (?) add optional `token` to be able to extend the lock.
struct NX_VMS_API VirtualDeviceLock: FlexibleId
{
    /**%apidoc Lock timeout in milliseconds.
     * %example 60000
     */
    std::chrono::milliseconds ttlMs;

    /**%apidoc Id of a user to acquire the lock for. Should only be used by an administrator. */
    std::optional<nx::Uuid> userId;
};
#define VirtualDeviceLock_Fields FlexibleId_Fields(ttlMs)(userId)
QN_FUSION_DECLARE_FUNCTIONS(VirtualDeviceLock, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(VirtualDeviceLock, VirtualDeviceLock_Fields);

// TODO: (?) this can be done using `lock`
struct NX_VMS_API VirtualDeviceExtend: VirtualDeviceLock
{
    /**%apidoc Token acquired when the virtual Device was locked. */
    nx::Uuid token;
};
#define VirtualDeviceExtend_Fields VirtualDeviceLock_Fields(token)
QN_FUSION_DECLARE_FUNCTIONS(VirtualDeviceExtend, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(VirtualDeviceExtend, VirtualDeviceExtend_Fields);

struct NX_VMS_API VirtualDeviceRelease: FlexibleId
{
    /**%apidoc Token acquired when the virtual Device was locked. */
    nx::Uuid token;
};
#define VirtualDeviceRelease_Fields FlexibleId_Fields(token)
QN_FUSION_DECLARE_FUNCTIONS(VirtualDeviceRelease, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(VirtualDeviceRelease, VirtualDeviceRelease_Fields);

} // namespace nx::vms::api

namespace nx::vms::api {

/**%apidoc
 * Description of a single video file to be uploaded to a Virtual Device archive.
 */
struct NX_VMS_API VirtualDeviceArchiveUploadItem
{
    /**%apidoc
     * Client-visible file name. Returned in responses so the caller can match
     * uploads and statuses back to the original request. The server may reuse
     * the same upload for different filenames if the file content is identical.
     */
    std::string filename;

    /**%apidoc:integer
     * File size in bytes.
     * %example 10000
     */
    double sizeB{};

    /**%apidoc:base64
     * MD5 checksum of the full file content, base64-encoded.
     * %example 0
     */
    std::string md5{};

    /**%apidoc
     * UTC time in milliseconds since Unix epoch of the first frame in the file.
     * Used by Server to lock the corresponding Archive period during the Upload.
     * %example 0
     */
    std::chrono::milliseconds startTimeMs{};

    /**%apidoc
     * Duration of the video content in milliseconds.
     * Used by Server to lock the corresponding Archive period during the Upload.
     * %example 60000
     */
    std::chrono::milliseconds durationMs{};

    /**%apidoc[opt]
     * Archive time offset in milliseconds relative to startTimeMs.
     * Defaults to 0, meaning the chunk is placed at startTimeMs.
     * Archive startTimeMs = startTimeMs + archiveOffsetMs.
     */
    std::chrono::milliseconds archiveOffsetMs{0};

    /**%apidoc[opt]:integer
     * Chunk size in bytes for uploading. Assigned by the server if not provided.
     */
    std::optional<double> chunkSizeB;

    /**%apidoc[opt]
     * Ignore any existing cached content for this file and require a fresh upload
     * from the client. This flag does not modify or overwrite existing archive footage.
     */
    bool forceUpload = false;
};
#define VirtualDeviceFileInfo_Fields \
    (filename)(sizeB)(md5)(startTimeMs)(durationMs)(archiveOffsetMs)(chunkSizeB)(forceUpload)
QN_FUSION_DECLARE_FUNCTIONS(VirtualDeviceArchiveUploadItem, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(VirtualDeviceArchiveUploadItem, VirtualDeviceFileInfo_Fields);

struct NX_VMS_API VirtualDeviceCreateUploads
{
    /**%apidoc
     * Virtual Device id, can be obtained from "id" field via
     * <code>GET /rest/v{3-}/devices/&ast;/virtual</code>.
     */
    std::string deviceId;

    /**%apidoc
     * Video files requested for upload to the archive. Multiple archive chunks may
     * reference the same file; items with identical VideoFileMetaInfo may share a
     * single upload on the server.
     */
    std::vector<VirtualDeviceArchiveUploadItem> items;
};
#define VirtualDeviceCreateUploads_Fields (deviceId)(items)
QN_FUSION_DECLARE_FUNCTIONS(VirtualDeviceCreateUploads, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(VirtualDeviceCreateUploads, VirtualDeviceCreateUploads_Fields);

struct NX_VMS_API StatusForVirtualDeviceUpload
{
    /**%apidoc
     * Filename as provided in the create-uploads request. Returned so the caller can
     * correlate this result with the original item and track it together with uploadId
     * and/or fileId.
     */
    std::string filename;

    /**%apidoc
     * Archive start time in milliseconds since Unix epoch for the resulting chunk.
     */
    std::chrono::milliseconds startTimeMs{};

    /**%apidoc
     * Duration in milliseconds of the resulting archive chunk.
     * %example 20000
     */
    std::chrono::milliseconds durationMs{};

    std::optional<std::string> _error;

    /**%apidoc
     * Identifier of this upload operation. Uniquely identifies both the file content
     * and the corresponding archive write for this item.
     */
    nx::Uuid uploadId;

    /**%apidoc:integer
     * Chunk size in bytes expected by the server when data is uploaded for this item.
     * Meaningful only when cached == false.
     * %example 5242880
     */
    double chunkSizeB = {};


    /**%apidoc
     * Identifier of the file shared between items in the request.
     * In the common one-file-per-item case it equals uploadId and can be ignored.
     * When a single file is reused by multiple items, fileId can be used instead
     * of uploadId as a stable handle for that shared content.
     */
    nx::Uuid fileId;

    /**%apidoc
     * Percent of file data already stored on the server for this upload, [0-100].
     */
    int uploadProgressPercent = 0;

    /**%apidoc
     * Percent of writing this upload into the archive, 0-100.
     * May be absent when this information is not available (for example,
     * when status is requested by fileId instead of uploadId).
     */
    std::optional<int> archiveProgressPercent;

    /**%apidoc
     * Id of the user who created the upload. Visible only for users who have
     * permissions to modify the Device.
     */
    std::optional<nx::Uuid> ownerId;
};
#define StatusForVirtualDeviceUpload_Fields \
    (filename)(startTimeMs)(durationMs) \
    (_error)(uploadId)(chunkSizeB)(fileId)(uploadProgressPercent)(archiveProgressPercent)(ownerId)
QN_FUSION_DECLARE_FUNCTIONS(StatusForVirtualDeviceUpload, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(StatusForVirtualDeviceUpload, StatusForVirtualDeviceUpload_Fields);

struct NX_VMS_API VirtualDeviceUploadId
{
    /**%apidoc
     * Virtual Device id, can be obtained from "id" field via
     * <code>GET /rest/v{3-}/devices/&ast;/virtual</code>.
     */
    std::string deviceId;

    /**%apidoc
     * Identifier of the upload operation. In advanced cases this may also be a fileId
     * shared by multiple uploads that use the same file content.
     */
    nx::Uuid uploadId;

    /**%apidoc[opt]
     * Zero-based index of this binary chunk within the uploaded file.
     * %example 1
     */
    int chunk = 0;
};
#define VirtualDeviceUploadId_Fields (deviceId)(uploadId)(chunk)
QN_FUSION_DECLARE_FUNCTIONS(VirtualDeviceUploadId, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(VirtualDeviceUploadId, VirtualDeviceUploadId_Fields);

} // namespace nx::vms::api
