// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QStringList>

#include <nx/reflect/enum_instrument.h>
#include <nx/string.h>
#include <nx/utils/json/flags.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/api/types/audit_record_type.h>
#include <nx/vms/api/types/motion_types.h>
#include <nx/vms/api/types/resource_types.h>

namespace Qn {
    Q_NAMESPACE_EXPORT(NX_VMS_COMMON_API)

    NX_REFLECTION_ENUM(ExtrapolationMode,
        ConstantExtrapolation,
        LinearExtrapolation,
        PeriodicExtrapolation
    )
    Q_ENUM_NS(ExtrapolationMode)

    NX_REFLECTION_ENUM(BackupState,
        BackupState_None = 0,
        BackupState_InProgress = 1
    )

    enum Projection {
        RectilinearProjection,
        EquirectangularProjection
    };

    NX_REFLECTION_ENUM(PanicMode,
        PM_None = 0,
        PM_BusinessEvents = 1
    )

    NX_REFLECTION_ENUM(ConnectionRole,
        CR_Default,         /// In client this flag is sufficient to receive both archive and live video
        CR_LiveVideo,
        CR_SecondaryLiveVideo,
        CR_Archive,
        CR_SeamlessArchive
    )

    NX_VMS_COMMON_API nx::vms::api::StreamIndex toStreamIndex(ConnectionRole role);

    Qn::ConnectionRole fromStreamIndex(nx::vms::api::StreamIndex streamIndex);

    // TODO: #sivanov Split to server-only and client-only flags as they are always local.
    enum ResourceFlag
    {
        none = 0, /**< No flags. */
        network = 1 << 0, /**< Has ip and mac. */
        url = 1 << 1, /**< Has url, e.g. file name. */
        media = 1 << 2,
        playback = 1 << 3, /**< Something playable (not real time and not a single shot). */
        video = 1 << 4,
        audio = 1 << 5,
        live = 1 << 6,

        still_image = 1 << 7, /**< Still image device. */
        local = 1 << 8, /**< Local client resource. */
        server = 1 << 9, /**< Server resource. */
        remote = 1 << 10, /**< Remote (on-server) resource. */

        layout = 1 << 11, /**< Layout resource. */
        user = 1 << 12, /**< User resource. */
        utc = 1 << 13, /**< Uses UTC-based timing. */
        periods = 1 << 14, /**< Has recorded periods. */
        motion = 1 << 15, /**< Has motion */
        sync = 1 << 16, /**< Can be used in sync playback mode. */

        // Server-only flag.
        foreigner = 1 << 17, /**< Belongs to other entity, e.g., camera on another server */

        // Client-only flag.
        /**
         * Fake server (belonging to other system) / Intercom layout, while it is local / User /
         * Desktop camera preloader (before it is updated by server, by creation real Desktop
         * camera).
         */
        fake = 1 << 18,

        videowall = 1 << 19, /**< Videowall resource */
        desktop_camera = 1 << 20, /**< Desktop Camera resource */

        // Server-only flag.
        need_recreate = 1 << 21, /**< Camera discovery internal purpose. Server-only flag. */

        // Server-only flag.
        search_upd_only = 1 << 22, /**< Disable to insert new resource during discovery process,
                                        allow update only */

        io_module = 1 << 23, /**< I/O module device (camera subtype) */
        read_only = 1 << 24, /**< Resource is read-only by design. */

        // Server-only flag.
        storage_fastscan = 1 << 25, /**< Fast scan for storage in progress */

        // Client-only flag.
        exported = 1 << 26, /**< Exported media file. */

        removed = 1 << 27, /**< Resource is already removed from the resource pool. */

        virtual_camera = 1 << 28, /**< Virtual camera resource. */

        /**
         * Cross-system Resource, e.g. Camera from another System or it's parent Server. Cloud
         * Layouts are also considered as cross-system Resources.
         */
        cross_system = 1 << 29,

        /**
         * Do not generate periodic(auto) thumbnails for camera
         */
        no_periodic_thumbnails = 1 << 30,

        local_media = local | media | url,
        exported_media = local_media | exported,

        local_layout = local | layout,
        exported_layout = local_layout | url | exported,
        local_intercom_layout = Qn::layout | Qn::local | Qn::fake,

        local_server = local | server,
        remote_server = remote | server,

        live_cam = utc | sync | live | media | video, // don't set w/o `local` or `remote` flag
        local_live_cam = live_cam | local | network,
        server_live_cam = live_cam | remote, // | network,
        local_video = local_media | video | audio, /**< Local media file. */
        local_image = local_media | still_image, /**< Local still image file. */

        web_page = url | remote, /**< Web-page resource */
    };
    Q_DECLARE_FLAGS(ResourceFlags, ResourceFlag)
    Q_FLAG_NS(ResourceFlags)
    Q_DECLARE_OPERATORS_FOR_FLAGS(ResourceFlags)

    // TODO: Move to the client code.
    /** Level of detail for displaying resource info. */
    enum ResourceInfoLevel
    {
        /**%apidoc[unused] */
        RI_Invalid,

        /**%apidoc
         * Only resource name
         * %caption nameOnly
         */
        RI_NameOnly,

        /**%apidoc
         * Resource name and url (if exist)
         * %caption withUrl
         */
        RI_WithUrl,

        /**%apidoc
         * All info
         * %caption full
         */
        RI_FullInfo
    };

    template<typename Visitor>
    constexpr auto nxReflectVisitAllEnumItems(ResourceInfoLevel*, Visitor&& visitor)
    {
        using Item = nx::reflect::enumeration::Item<ResourceInfoLevel>;
        return visitor(
            Item{RI_Invalid, ""},
            Item{RI_NameOnly, "nameOnly"},
            Item{RI_WithUrl, "withUrl"},
            Item{RI_FullInfo, "full"}
        );
    }

    NX_REFLECTION_ENUM_CLASS(StatusChangeReason,
        Local,
        GotFromRemotePeer
    )

    enum IOPortType
    {
        /**%apidoc
         * %caption Unknown
         */
        PT_Unknown = 0x0,

        /**%apidoc
         * %caption Disabled
         */
        PT_Disabled = 0x1,

        /**%apidoc
         * %caption Input
         */
        PT_Input = 0x2,

        /**%apidoc
         * %caption Output
         */
        PT_Output = 0x4
    };

    template<typename Visitor>
    constexpr auto nxReflectVisitAllEnumItems(IOPortType*, Visitor&& visitor)
    {
        using Item = nx::reflect::enumeration::Item<IOPortType>;
        return visitor(
            Item{PT_Unknown, "Unknown"},
            Item{PT_Disabled, "Disabled"},
            Item{PT_Input, "Input"},
            Item{PT_Output, "Output"}
        );
    }

    Q_DECLARE_FLAGS(IOPortTypes, IOPortType)
    Q_DECLARE_OPERATORS_FOR_FLAGS(IOPortTypes)

    NX_REFLECTION_ENUM(LegacyAuditRecordType,
        AR_NotDefined = static_cast<int>(nx::vms::api::AuditRecordType::notDefined),
        AR_UnauthorizedLogin = static_cast<int>(nx::vms::api::AuditRecordType::unauthorizedLogin),
        AR_Login = static_cast<int>(nx::vms::api::AuditRecordType::login),
        AR_UserUpdate = static_cast<int>(nx::vms::api::AuditRecordType::userUpdate),
        AR_ViewLive = static_cast<int>(nx::vms::api::AuditRecordType::viewLive),
        AR_ViewArchive = static_cast<int>(nx::vms::api::AuditRecordType::viewArchive),
        AR_ExportVideo = static_cast<int>(nx::vms::api::AuditRecordType::exportVideo),
        AR_CameraUpdate = static_cast<int>(nx::vms::api::AuditRecordType::deviceUpdate),
        AR_SystemNameChanged = static_cast<int>(nx::vms::api::AuditRecordType::siteNameChanged),
        AR_SystemmMerge = static_cast<int>(nx::vms::api::AuditRecordType::siteMerge),
        AR_SettingsChange = static_cast<int>(nx::vms::api::AuditRecordType::settingsChange),
        AR_ServerUpdate = static_cast<int>(nx::vms::api::AuditRecordType::serverUpdate),
        AR_BEventUpdate = static_cast<int>(nx::vms::api::AuditRecordType::eventUpdate),
        AR_EmailSettings = static_cast<int>(nx::vms::api::AuditRecordType::emailSettings),
        AR_CameraRemove = static_cast<int>(nx::vms::api::AuditRecordType::deviceRemove),
        AR_ServerRemove = static_cast<int>(nx::vms::api::AuditRecordType::serverRemove),
        AR_BEventRemove = static_cast<int>(nx::vms::api::AuditRecordType::eventRemove),
        AR_UserRemove = static_cast<int>(nx::vms::api::AuditRecordType::userRemove),
        AR_DatabaseRestore = static_cast<int>(nx::vms::api::AuditRecordType::databaseRestore),
        AR_CameraInsert = static_cast<int>(nx::vms::api::AuditRecordType::deviceInsert),
        AR_UpdateInstall = static_cast<int>(nx::vms::api::AuditRecordType::updateInstall),
        AR_StorageInsert = static_cast<int>(nx::vms::api::AuditRecordType::storageInsert),
        AR_StorageUpdate = static_cast<int>(nx::vms::api::AuditRecordType::storageUpdate),
        AR_StorageRemove = static_cast<int>(nx::vms::api::AuditRecordType::storageRemove),
        AR_MitmAttack = static_cast<int>(nx::vms::api::AuditRecordType::mitmAttack),
        AR_CloudBind = static_cast<int>(nx::vms::api::AuditRecordType::cloudBind),
        AR_CloudUnbind = static_cast<int>(nx::vms::api::AuditRecordType::cloudUnbind),
        AR_ProxySession = static_cast<int>(nx::vms::api::AuditRecordType::proxySession)
    )
    Q_DECLARE_FLAGS(LegacyAuditRecordTypes, LegacyAuditRecordType)
    Q_DECLARE_OPERATORS_FOR_FLAGS(LegacyAuditRecordTypes)

    enum IODefaultState {
        IO_OpenCircuit,
        IO_GroundedCircuit
    };

    template<typename Visitor>
    constexpr auto nxReflectVisitAllEnumItems(IODefaultState*, Visitor&& visitor)
    {
        using Item = nx::reflect::enumeration::Item<IODefaultState>;
        return visitor(
            Item{IO_OpenCircuit, "Open Circuit"},
            Item{IO_GroundedCircuit, "Grounded circuit"}
        );
    }

    enum TimePeriodContent {
        RecordingContent,
        MotionContent,
        AnalyticsContent,

        TimePeriodContentCount
    };
    Q_ENUM_NS(TimePeriodContent)

    using StreamQuality = nx::vms::api::StreamQuality;

    enum class BitrateControl {
        undefined,
        cbr,
        vbr
    };

    enum class EncodingPriority {
        undefined,
        framerate,
        compressionLevel
    };

    enum class EntropyCoding {
        undefined,
        cavlc,
        cabac
    };

    NX_REFLECTION_ENUM(StatisticsDeviceType,
        StatisticsCPU = 0, /**< CPU load in percents. */
        StatisticsRAM = 1, /**< RAM load in percents. */
        StatisticsHDD = 2, /**< HDD load in percents. */
        StatisticsNETWORK = 3 /**< Network load in percent. */
    )

    using CameraStatusFlag = nx::vms::api::CameraStatusFlag;
    using CameraStatusFlags = nx::vms::api::CameraStatusFlags;

    using RecordingType = nx::vms::api::RecordingType;
    using RecordingMetadataType = nx::vms::api::RecordingMetadataType;
    using RecordingMetadataTypes = nx::vms::api::RecordingMetadataTypes;

    NX_REFLECTION_ENUM(LicenseType,
        /**
         * Includes free license, which each user can activate once, and demo licenses, which are
         * provided by the support team on user's request.
         */
        LC_Trial,

        LC_Analog,
        LC_Professional,
        LC_Edge,
        LC_VMAX,
        LC_AnalogEncoder,
        LC_VideoWall,

        /**
         * I/O Modules license.
         * Needs to be activated to enable I/O module features. One license channel per one module.
         */
        LC_IO,

        /**
         * Like a professional license.
         * Only one license key per system (not server). If systems are merged and each of them had
         * some start licenses originally, new merged system will only take one start license (the
         * one with bigger number of channels). Other start licenses will become invalid.
         */
        LC_Start,

        /**
         * A camera with Free License can be recorded without License activation. It is always
         * available for use.
         */
        LC_Free,

        /**
         * For all new DVR/NVR devices except of VMAX
         */
        LC_Bridge,

        /**
         * Like a professional license.
         * Only one license key per system (not server). If systems are merged and each of them had
         * some NVR licenses originally, new merged system will only take one NVR license (the one
         * with bigger number of channels). Other NVR licenses will become invalid.
         */
        LC_Nvr,

        /**
         * Saas local recording service.
         */
        LC_SaasLocalRecording,

        /**
         * Invalid license. Required when the correct license type is not known in current version.
         */
        LC_Invalid,

        LC_Count
    )

    using FailoverPriority = nx::vms::api::FailoverPriority;

    enum StorageInitResult
    {
        /**%apidoc
         * %caption Ok
         */
        StorageInit_Ok,

        /**%apidoc
         * %caption CreateFailed
         */
        StorageInit_CreateFailed,

        /**%apidoc
         * %caption InitFailed_WrongPath
         */
        StorageInit_WrongPath,

        /**%apidoc
         * %caption InitFailed_WrongAuth
         */
        StorageInit_WrongAuth,
    };

    template<typename Visitor>
    constexpr auto nxReflectVisitAllEnumItems(StorageInitResult*, Visitor&& visitor)
    {
        using Item = nx::reflect::enumeration::Item<StorageInitResult>;
        return visitor(
            Item{StorageInit_Ok, "Ok"},
            Item{StorageInit_CreateFailed, "CreateFailed"},
            Item{StorageInit_WrongPath, "InitFailed_WrongPath"},
            Item{StorageInit_WrongAuth, "InitFailed_WrongAuth"}
        );
    }

    /**
     * Flags describing the actions permitted for the user/role to do with the selected resource.
     * Calculated in runtime.
     */
    NX_REFLECTION_ENUM(Permission,
        //-----------------------------------------------------------------------------------------
        //  Generic resource permissions.

        // No access.
        NoPermissions = 0,

        // Generic read access. Having this permission doesn't necessary mean that all
        // information is readable. See resource-specific permissions below. By default this means
        // that this resource will be available in server api replies (e.g. FullInfo)
        ReadPermission = 1 << 0,

        // Generic write access. Having this permission doesn't necessary mean that all
        // information is writable. See resource-specific permissions below. Actual only on the
        // client side. By default this means we can modify the resource somehow.
        WritePermission = 1 << 1,

        // Generic save access. Resource can be saved to the server database. Actual both on the
        // client and the server side.
        SavePermission = 1 << 2,

        // Generic delete permission. Resource can be deleted from the server database.
        RemovePermission = 1 << 3,

        // Permission to edit resource's name.
        WriteNamePermission = 1 << 4,

        // Alias for common set of generic permissions.
        ReadWriteSavePermission = ReadPermission | WritePermission | SavePermission,

        // Alias for generic edit set of permissions.
        GenericEditPermissions = WritePermission | WriteNamePermission | SavePermission,

        // Alias for full set of generic permissions.
        FullGenericPermissions = ReadWriteSavePermission | RemovePermission | WriteNamePermission,

        /**
         * Generic permission to view actual resource content.
         * Actually that means we can open widget with this resource's content on the scene.
         *  - cameras & devices: can be opened on the scene with live and/or archive access
         *      (comes with ViewLivePermission or ViewFootagePermission or both)
         *  - servers: health monitor widgets can be opened on the scene
         *  - web pages: can be opened on the scene
         *  - layouts: can be opened on the scene or placed on a showreel
         *      (doesn't mean that layout items also have this permission)
         *  - videowalls: can be opened on the scene as a videowall review layout
         */
        ViewContentPermission = 1 << 5,

        //-----------------------------------------------------------------------------------------

        //-----------------------------------------------------------------------------------------
        // Webpage-specific permissions.

        FullWebPagePermissions = FullGenericPermissions | ViewContentPermission,

        //-----------------------------------------------------------------------------------------

        //-----------------------------------------------------------------------------------------
        // Server-specific permissions.

        // Full set of permissions which can be available for the server resource.
        FullServerPermissions = FullGenericPermissions | ViewContentPermission,

        //-----------------------------------------------------------------------------------------

        //-----------------------------------------------------------------------------------------
        // Layout-specific permissions.

         // Permission to add or remove items from a layout.
        AddRemoveItemsPermission = 1 << 6,

        // Permission to setup layout background or set locked flag.
        EditLayoutSettingsPermission = 1 << 7,

        // Permission set to modify without saving.
        ModifyLayoutPermissions = ReadPermission
            | WritePermission
            | ViewContentPermission
            | AddRemoveItemsPermission,

        // Full set of permissions which can be available for the layout resource.
        FullLayoutPermissions = FullGenericPermissions
            | ViewContentPermission
            | AddRemoveItemsPermission
            | EditLayoutSettingsPermission,

        //-----------------------------------------------------------------------------------------

        //-----------------------------------------------------------------------------------------
        // User-specific permissions.

        // Permission to enable or disable digest.
        WriteDigestPermission = 1 << 8,

        // Permission to edit associated password.
        WritePasswordPermission = 1 << 9,

        // Permission to edit access rights.
        WriteAccessRightsPermission = 1 << 10,

        // Permission to edit user's email.
        WriteEmailPermission = 1 << 11,

        // Permission to edit user's full name.
        WriteFullNamePermission = 1 << 12,

        // Permission to edit user's locale.
        WriteLocalePermission = 1 << 13,

        // Full set of permissions which can be available for the user resource.
        FullUserPermissions = FullGenericPermissions
            | WritePasswordPermission
            | WriteDigestPermission
            | WriteAccessRightsPermission
            | WriteFullNamePermission
            | WriteEmailPermission
            | WriteLocalePermission,

        //-----------------------------------------------------------------------------------------

        //-----------------------------------------------------------------------------------------
        // Media-specific permissions.

        // Permission to view camera's live stream.
        ViewLivePermission = 1 << 14,

        // Permission to view camera's footage.
        ViewFootagePermission = 1 << 15,

        // Permission to export video parts.
        ExportPermission = 1 << 16,

        // Permission to use camera's PTZ controls.
        WritePtzPermission = 1 << 17,

        // Permission to view camera bookmarks.
        ViewBookmarksPermission = 1 << 18,

        // Permission to edit camera bookmarks.
        ManageBookmarksPermission = 1 << 19,

        // Permission to toggle I/O module inputs.
        DeviceInputPermission = 1 << 20,

        // Permission to invoke soft triggers.
        SoftTriggerPermission = 1 << 21,

        // Permission to use two-way audio.
        TwoWayAudioPermission = 1 << 22,

        // Permission to play sound from a camera
        // (used in combination with ViewLivePermission and/or ViewFootagePermission).
        PlayAudioPermission = 1 << 23,

        // Permission granted by userInput access right.
        UserInputPermissions = WritePtzPermission
            | DeviceInputPermission
            | SoftTriggerPermission
            | TwoWayAudioPermission,

        //-----------------------------------------------------------------------------------------

        //-----------------------------------------------------------------------------------------
        // Mode-specific permissions.

        // Layout access permission for the running videowall instance.
        VideoWallLayoutPermissions = ModifyLayoutPermissions,

        // Locked layout access permission for the running videowall instance.
        VideoWallLockedLayoutPermissions = ModifyLayoutPermissions
            &~ (AddRemoveItemsPermission | WriteNamePermission),

        // Media access permission for the running videowall instance.
        // PTZ is intended here.
        VideoWallMediaPermissions = ReadPermission
            | ViewContentPermission
            | ViewLivePermission
            | ViewFootagePermission
            | WritePtzPermission,

        //-----------------------------------------------------------------------------------------

        // All eventually possible permissions.
        AllPermissions = 0xFFFFFFFF
    )

    Q_DECLARE_FLAGS(Permissions, Permission)
    Q_DECLARE_OPERATORS_FOR_FLAGS(Permissions)

    NX_REFLECTION_ENUM_CLASS(FisheyeCameraProjection,
        equidistant,
        stereographic,
        equisolid
    )

    inline size_t qHash(FisheyeCameraProjection value, size_t seed = 0)
    {
        return ::qHash(int(value), seed);
    }

    /**
     * Helper function that can be used to 'place' macros into Qn namespace.
     */
    template<class T>
    const T &_id(const T &value) { return value; }

    const static QLatin1String kWallpapersFolder("wallpapers");

    inline QString toString(nx::vms::api::StreamIndex streamIndex)
    {
        switch (streamIndex)
        {
            case nx::vms::api::StreamIndex::primary:
                return "primary";
            case nx::vms::api::StreamIndex::secondary:
                return "secondary";
            default:
                return "undefined";
        }
    }

    /** Debug representation. */
    NX_VMS_COMMON_API QString toString(Qn::TimePeriodContent value);

} // namespace Qn

using nx::vms::api::GlobalPermission;
using nx::vms::api::GlobalPermissions;
