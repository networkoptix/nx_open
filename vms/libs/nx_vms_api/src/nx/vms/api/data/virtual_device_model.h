// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/fusion/model_functions_fwd.h>
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
    QnUuid userId;

    /**%apidoc Lock token. */
    QnUuid token;

    /**%apidoc Lock timeout in milliseconds. */
    std::chrono::milliseconds ttlMs;

    /**%apidoc Consume progress, if ongoing. */
    std::optional<int> progress = 0;
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
    QnUuid token;

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

    /**%apidoc Id of a user to acquire the lock for. Should only be used by an admininstrator. */
    std::optional<QnUuid> userId;
};
#define VirtualDeviceLock_Fields FlexibleId_Fields(ttlMs)(userId)
QN_FUSION_DECLARE_FUNCTIONS(VirtualDeviceLock, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(VirtualDeviceLock, VirtualDeviceLock_Fields);

// TODO: (?) this can be done using `lock`
struct NX_VMS_API VirtualDeviceExtend: VirtualDeviceLock
{
    /**%apidoc Token acquired when the virtual Device was locked. */
    QnUuid token;
};
#define VirtualDeviceExtend_Fields VirtualDeviceLock_Fields(token)
QN_FUSION_DECLARE_FUNCTIONS(VirtualDeviceExtend, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(VirtualDeviceExtend, VirtualDeviceExtend_Fields);

struct NX_VMS_API VirtualDeviceRelease: FlexibleId
{
    /**%apidoc Token acquired when the virtual Device was locked. */
    QnUuid token;
};
#define VirtualDeviceRelease_Fields FlexibleId_Fields(token)
QN_FUSION_DECLARE_FUNCTIONS(VirtualDeviceRelease, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(VirtualDeviceRelease, VirtualDeviceRelease_Fields);

} // namespace nx::vms::api
