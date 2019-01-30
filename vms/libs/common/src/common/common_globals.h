#pragma once

#include <limits>

#include <QtCore/QtGlobal>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/serialization_format.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/api/types/motion_types.h>
#include <nx/vms/api/types/resource_types.h>

#ifdef THIS_BLOCK_IS_REQUIRED_TO_MAKE_FILE_BE_PROCESSED_BY_MOC_DO_NOT_DELETE
Q_OBJECT
#endif
QN_DECLARE_METAOBJECT_HEADER(Qn,
    ExtrapolationMode CameraCapability PtzObjectType PtzCommand PtzDataField PtzCoordinateSpace
    StreamFpsSharingMethod TimePeriodContent SystemComponent
    ConnectionRole ResourceStatus
    PanicMode RebuildState BackupState PeerType StatisticsDeviceType
    StorageInitResult IOPortType IODefaultState AuditRecordType AuthResult
    RebuildAction BackupAction MediaStreamEvent StreamIndex
    ResourceStatus StatusChangeReason
    Permission UserRole ConnectionResult
    ,
    Borders Corners ResourceFlags CameraCapabilities PtzDataFields
    ServerFlags IOPortTypes
    Permissions StorageStatuses
    )

    enum ExtrapolationMode {
        ConstantExtrapolation,
        LinearExtrapolation,
        PeriodicExtrapolation
    };

    enum CameraCapability {
        NoCapabilities                      = 0x000,
        PrimaryStreamSoftMotionCapability   = 0x004,
        InputPortCapability                 = 0x008,
        OutputPortCapability                = 0x010,
        ShareIpCapability                   = 0x020,
        AudioTransmitCapability             = 0x040,
        RemoteArchiveCapability             = 0x100,
        SetUserPasswordCapability           = 0x200, //< Can change password on a camera.
        IsDefaultPasswordCapability         = 0x400, //< Camera has default password now.
        IsOldFirmwareCapability             = 0x800, //< Camera has too old firmware.
        CustomMediaUrlCapability            = 0x1000, //< Camera's streams are editable.
        IsPlaybackSpeedSupported            = 0x2000, //< For NVR which support playback speed 1,2,4 e.t.c natively.
        DeviceBasedSync                     = 0x4000, //< For NVR if channels are depend on each other and can play synchronously only.
        DualStreamingForLiveOnly            = 0x8000,
        customMediaPortCapability           = 0x10000, //< Camera's media streams port are editable.
        CameraTimeCapability                = 0x20000, //< Camera sends absolute timestamps in media stream
        FixedQualityCapability              = 0x40000, //< Camera does not allow to change stream quality/fps
    };
    Q_DECLARE_FLAGS(CameraCapabilities, CameraCapability)
    Q_DECLARE_OPERATORS_FOR_FLAGS(CameraCapabilities)

    enum PtzCommand {
        ContinuousMovePtzCommand,
        ContinuousFocusPtzCommand,
        AbsoluteDeviceMovePtzCommand,
        AbsoluteLogicalMovePtzCommand,
        ViewportMovePtzCommand,

        GetDevicePositionPtzCommand,
        GetLogicalPositionPtzCommand,
        GetDeviceLimitsPtzCommand,
        GetLogicalLimitsPtzCommand,
        GetFlipPtzCommand,

        CreatePresetPtzCommand,
        UpdatePresetPtzCommand,
        RemovePresetPtzCommand,
        ActivatePresetPtzCommand,
        GetPresetsPtzCommand,

        CreateTourPtzCommand,
        RemoveTourPtzCommand,
        ActivateTourPtzCommand,
        GetToursPtzCommand,

        GetActiveObjectPtzCommand,
        UpdateHomeObjectPtzCommand,
        GetHomeObjectPtzCommand,

        GetAuxiliaryTraitsPtzCommand,
        RunAuxiliaryCommandPtzCommand,

        GetDataPtzCommand,

        RelativeMovePtzCommand,
        RelativeFocusPtzCommand,

        InvalidPtzCommand = -1
    };

    enum PtzDataField {
        NoPtzFields = 0,
        CapabilitiesPtzField = 1 << 0,
        DevicePositionPtzField = 1 << 1,
        LogicalPositionPtzField = 1 << 2,
        DeviceLimitsPtzField = 1 << 3,
        LogicalLimitsPtzField = 1 << 4,
        FlipPtzField = 1 << 5,
        PresetsPtzField = 1 << 6,
        ToursPtzField = 1 << 7,
        ActiveObjectPtzField = 1 << 8,
        HomeObjectPtzField = 1 << 9,
        AuxiliaryTraitsPtzField = 1 << 10,
        AllPtzFields = -1 //< All ones.
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PtzDataField)

    Q_DECLARE_FLAGS(PtzDataFields, PtzDataField)
    Q_DECLARE_OPERATORS_FOR_FLAGS(PtzDataFields)

    enum RebuildState {
        RebuildState_None        = 1,
        RebuildState_FullScan    = 2,
        RebuildState_PartialScan = 3
        //RebuildState_Canceled    = 4,
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(RebuildState)

    enum BackupState {
        BackupState_None        = 0,
        BackupState_InProgress  = 1
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(BackupState)

    enum PtzCoordinateSpace {
        DevicePtzCoordinateSpace,
        LogicalPtzCoordinateSpace,
        InvalidPtzCoordinateSpace
    };

    enum PtzObjectType {
        PresetPtzObject,
        TourPtzObject,

        InvalidPtzObject = -1
    };

    enum Projection {
        RectilinearProjection,
        EquirectangularProjection
    };

    enum StreamFpsSharingMethod {
        /** If second stream is running whatever fps it has, first stream can get maximumFps - secondstreamFps. */
        BasicFpsSharing,

        /** If second stream is running whatever megapixel it has, first stream can get maxMegapixels - secondstreamPixels. */
        PixelsFpsSharing,

        /** Second stream does not affect first stream's fps. */
        NoFpsSharing
    };

    using MotionType = nx::vms::api::MotionType;
    using MotionTypes = nx::vms::api::MotionTypes;

    enum PanicMode {
        PM_None = 0,
        PM_BusinessEvents = 1,
        PM_User = 2
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PanicMode)

    enum ConnectionRole {
        CR_Default,         /// In client this flag is sufficient to receive both archive and live video
        CR_LiveVideo,
        CR_SecondaryLiveVideo,
        CR_Archive
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Qn::ConnectionRole)

    // TODO: #GDM split to server-only and client-only flags as they are always local
    enum ResourceFlag
    {
        network                     = 0x1,          /**< Has ip and mac. */
        url                         = 0x2,          /**< Has url, e.g. file name. */
        streamprovider              = 0x4,
        media                       = 0x8,

        playback                    = 0x10,         /**< Something playable (not real time and not a single shot). */
        video                       = 0x20,
        audio                       = 0x40,
        live                        = 0x80,

        still_image                 = 0x100,        /**< Still image device. */
        local                       = 0x200,        /**< Local client resource. */
        server                      = 0x400,        /**< Server resource. */
        remote                      = 0x800,        /**< Remote (on-server) resource. */

        layout                      = 0x1000,       /**< Layout resource. */
        user                        = 0x2000,       /**< User resource. */
        utc                         = 0x4000,       /**< Resource uses UTC-based timing. */
        periods                     = 0x8000,       /**< Resource has recorded periods. */

        motion                      = 0x10000,      /**< Resource has motion */
        sync                        = 0x20000,      /**< Resource can be used in sync playback mode. */

        /* Server-only flag. */
        foreigner                   = 0x40000,      /**< Resource belongs to other entity. E.g., camera on another server */

        /* Server-only flag. */
        no_last_gop                 = 0x80000,      /**< Do not use last GOP for this when stream is opened */

        /* Client-only flag */
        fake                        = 0x100000,     /**< Fake server (belonging to other system). */

        videowall                   = 0x200000,     /**< Videowall resource */
        desktop_camera              = 0x400000,     /**< Desktop Camera resource */

        /* Server-only flag. */
        parent_change               = 0x800000,     /**< Camera discovery internal purpose. Server-only flag. */

        /* Client-only flag. */
        depend_on_parent_status     = 0x1000000,    /**< Resource status depend on parent resource status. */

        /* Server-only flag. */
        search_upd_only             = 0x2000000,    /**< Disable to insert new resource during discovery process, allow update only */

        io_module                   = 0x4000000,    /**< It's I/O module camera (camera subtype) */
        read_only                   = 0x8000000,    /**< Resource is read-only by design, e.g. server in safe mode. */

        storage_fastscan            = 0x10000000,   /**< Fast scan for storage in progress */

        /* Client-only flag. */
        exported                    = 0x20000000,   /**< Exported media file. */
        removed                     = 0x40000000,   /**< resource removed from pool. */

        wearable_camera             = 0x80000000,   /**< Wearable camera resource. */


        local_media = local | media | url,
        exported_media = local_media | exported,

        local_layout = local | layout,
        exported_layout = local_layout | url | exported,

        local_server = local | server,
        remote_server = remote | server,
        safemode_server = read_only | server,

        live_cam = utc | sync | live | media | video | streamprovider, // don't set w/o `local` or `remote` flag
        local_live_cam = live_cam | local | network,
        server_live_cam = live_cam | remote,// | network,
        server_archive = remote | media | video | audio | streamprovider,
        local_video = local_media | video | audio | streamprovider,     /**< Local media file. */
        local_image = local_media | still_image | streamprovider,    /**< Local still image file. */

        web_page = url | remote,   /**< Web-page resource */
        fake_server = remote_server | fake
    };
    Q_DECLARE_FLAGS(ResourceFlags, ResourceFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(ResourceFlags)

    enum ResourceStatus
    {
        Offline,
        Unauthorized,
        Online,
        Recording,
        NotDefined,
        /*! Applies only to a server resource. A server is incompatible only when it has system name different
         * from the current or it has incompatible protocol version.
         * \note Incompatible server is not the same as fake server which is create in the client by
         * QnIncompatibleServerWatcher. Fake servers can also have Unauthorized status.
         * So if you want to check if the server is fake use QnMediaServerResource::isFakeServer().
         */
        Incompatible,

        AnyStatus
    };

    QString toString(ResourceStatus status);

    /** Level of detail for displaying resource info. */
    enum ResourceInfoLevel
    {
        RI_Invalid,
        RI_NameOnly,       /**< Only resource name */
        RI_WithUrl,        /**< Resource name and url (if exist) */
        RI_FullInfo        /**< All info */
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResourceInfoLevel)

    enum class StatusChangeReason
    {
        Local,
        GotFromRemotePeer
    };

    QString toString(StatusChangeReason reason);


    enum IOPortType {
        PT_Unknown  = 0x0,
        PT_Disabled = 0x1,
        PT_Input    = 0x2,
        PT_Output   = 0x4
    };
    Q_DECLARE_FLAGS(IOPortTypes, IOPortType)
    Q_DECLARE_OPERATORS_FOR_FLAGS(IOPortTypes)
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(IOPortType)

    enum AuditRecordType
    {
        AR_NotDefined        = 0x0000,
        AR_UnauthorizedLogin = 0x0001,
        AR_Login             = 0x0002,
        AR_UserUpdate        = 0x0004,
        AR_ViewLive          = 0x0008,
        AR_ViewArchive       = 0x0010,
        AR_ExportVideo       = 0x0020,
        AR_CameraUpdate      = 0x0040,
        AR_SystemNameChanged = 0x0080,
        AR_SystemmMerge      = 0x0100,
        AR_SettingsChange    = 0x0200,
        AR_ServerUpdate      = 0x0400,
        AR_BEventUpdate      = 0x0800,
        AR_EmailSettings     = 0x1000,
        AR_CameraRemove      = 0x2000,
        AR_ServerRemove      = 0x4000,
        AR_BEventRemove      = 0x8000,
        AR_UserRemove        = 0x10000,
        AR_BEventReset       = 0x20000,
        AR_DatabaseRestore   = 0x40000,
        AR_CameraInsert      = 0x80000,
        AR_UpdateInstall     = 0x100000
    };

    Q_DECLARE_FLAGS(AuditRecordTypes, AuditRecordType)
    Q_DECLARE_OPERATORS_FOR_FLAGS(AuditRecordTypes)
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(AuditRecordType)

    enum IODefaultState {
        IO_OpenCircuit,
        IO_GroundedCircuit
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(IODefaultState)

    enum TimePeriodContent {
        RecordingContent,
        MotionContent,
        AnalyticsContent,

        TimePeriodContentCount
    };

    enum SystemComponent {
        ServerComponent,
        ClientComponent,
        AnyComponent
    };

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

    enum StatisticsDeviceType {
        StatisticsCPU = 0,                /**< CPU load in percents. */
        StatisticsRAM = 1,                /**< RAM load in percents. */
        StatisticsHDD = 2,                /**< HDD load in percents. */
        StatisticsNETWORK = 3             /**< Network load in percent. */
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(StatisticsDeviceType)

    using CameraStatusFlag = nx::vms::api::CameraStatusFlag;
    using CameraStatusFlags = nx::vms::api::CameraStatusFlags;

    using RecordingType = nx::vms::api::RecordingType;

    enum TTHeaderFlag
    {
        TT_None          = 0x0,
        TT_ProxyToClient = 0x1
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(TTHeaderFlag)
    Q_DECLARE_FLAGS(TTHeaderFlags, TTHeaderFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(TTHeaderFlags)

    enum LicenseType
    {
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
         * Could not be activated on ARM devices.
         * Only one license key per system (not server). If systems are merged and each of them had some start licenses originally,
         * new merged system will only take one start license( the one with bigger channels). Other start licenses will become invalid.
         */
        LC_Start,

        /**
          * Camera with Free license can be recorded without license activation. It always available to use
          */
        LC_Free,

        /**
         * For all new DVR/NVR devices except of VMAX
         */
        LC_Bridge,

        /**
         * Invalid license. Required when the correct license type is not known in current version.
         */
        LC_Invalid,

        LC_Count
    };

    // All columns are sorted by database initially, except camera name and tags.
    enum BookmarkSortField
    {
        BookmarkName,
        BookmarkStartTime,
        BookmarkDuration,
        BookmarkCreationTime,   //< Sorted manually!
                                // TODO: #ynikitenkov add migration from older version to prevent
                                // empty/zero creation time. It would allow as to sort bookmarks
                                // by database, not manually, which is faster.
        BookmarkCreator,        //< Sorted manually!
        BookmarkTags,           //< Sorted manually!
        BookmarkCameraName,      //< Sorted manually!
        BookmarkCameraThenStartTime,
    };

    /**
    * Authentication error code
    */
    enum AuthResult
    {
        Auth_OK,            // OK
        Auth_WrongLogin,    // invalid login
        Auth_WrongInternalLogin, // invalid login used for internal auth scheme
        Auth_WrongDigest,   // invalid or empty digest
        Auth_WrongPassword, // invalid password
        Auth_Forbidden,     // no auth method found or custom auth scheme without login/password is failed
        Auth_PasswordExpired, // Password is expired
        Auth_LDAPConnectError,   // can't connect to the LDAP system to authenticate
        Auth_CloudConnectError,   // can't connect to the Cloud to authenticate
        Auth_DisabledUser,    // disabled user
        Auth_InvalidCsrfToken, // for cookie login
        Auth_LockedOut, //< locked out for a period of time.
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(AuthResult)
    QString toString(AuthResult value);
    QString toErrorMessage(AuthResult value);

    using FailoverPriority = nx::vms::api::FailoverPriority;

    // TODO: #MSAPI move to api/model or even to common_globals,
    // add lexical serialization (see QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS)
    //
    // Check serialization/deserialization in QnMediaServerConnection::doRebuildArchiveAsync
    // and in handler.
    //
    // And name them sanely =)
    enum RebuildAction
    {
        RebuildAction_ShowProgress,
        RebuildAction_Start,
        RebuildAction_Cancel
    };

    enum BackupAction
    {
        BackupAction_ShowProgress,
        BackupAction_Start,
        BackupAction_Cancel
    };


    /**
     * backup settings
     */

using CameraBackupQuality = nx::vms::api::CameraBackupQuality;
using CameraBackupQualities = nx::vms::api::CameraBackupQualities;

    enum StorageInitResult
    {
        StorageInit_Ok,
        StorageInit_CreateFailed,
        StorageInit_WrongPath,
        StorageInit_WrongAuth,
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(StorageInitResult)

    /**
     * Flags describing the actions permitted for the user/role to do with the selected resource.
     * Calculated in runtime.
     */
    enum Permission
    {
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

        // Alias for full set of generic permissions.
        FullGenericPermissions = ReadWriteSavePermission | RemovePermission | WriteNamePermission,

        /**
         * Generic permission to view actual resource content. Actually that means we can open
         * widget with this resource's content on the scene. For servers: health monitor access.
         */
        ViewContentPermission = 1 << 5,

        //-----------------------------------------------------------------------------------------

        //-----------------------------------------------------------------------------------------
        // Webpage-specific permissions.

        // Permission to view web page.
        ViewWebPagePermission = ViewContentPermission,

        //-----------------------------------------------------------------------------------------

        //-----------------------------------------------------------------------------------------
        // Server-specific permissions.

        // Permission to view health monitoring.
        ViewHealthMonitorPermission = ViewContentPermission,

        // Full set of permissions which can be available for the server resource.
        FullServerPermissions = FullGenericPermissions | ViewHealthMonitorPermission,

        //-----------------------------------------------------------------------------------------

        //-----------------------------------------------------------------------------------------
        // Layout-specific permissions.

         // Permission to add or remove items from a layout.
        AddRemoveItemsPermission = 1 << 6,

        // Permission to setup layout background or set locked flag.
        EditLayoutSettingsPermission = 1 << 7,

        // Permission set to modify without saving.
        ModifyLayoutPermission = ReadPermission | WritePermission | AddRemoveItemsPermission,

        // Full set of permissions which can be available for the layout resource.
        FullLayoutPermissions = FullGenericPermissions
            | AddRemoveItemsPermission
            | EditLayoutSettingsPermission,

        //-----------------------------------------------------------------------------------------

        //-----------------------------------------------------------------------------------------
        // User-specific permissions.

        // Permission to edit associated password.
        WritePasswordPermission = 1 << 8,

        // Permission to edit access rights.
        WriteAccessRightsPermission = 1 << 9,

        // Permission to edit user's email.
        WriteEmailPermission = 1 << 10,

        // Permission to edit user's full name.
        WriteFullNamePermission = 1 << 11,

        // Full set of permissions which can be available for the user resource.
        FullUserPermissions = FullGenericPermissions
            | WritePasswordPermission
            | WriteAccessRightsPermission
            | WriteFullNamePermission
            | WriteEmailPermission,

        //-----------------------------------------------------------------------------------------

        //-----------------------------------------------------------------------------------------
        // Media-specific permissions.

        // Permission to view camera's live stream.
        ViewLivePermission = 1 << 12,

        // Permission to view camera's footage.
        ViewFootagePermission = 1 << 13,

        // Permission to export video parts.
        ExportPermission = 1 << 14,

        // Permission to use camera's PTZ controls.
        WritePtzPermission = 1 << 15,

        //-----------------------------------------------------------------------------------------

        //-----------------------------------------------------------------------------------------
        // Mode-specific permissions.

        // Layout access permission for the running videowall instance.
        VideoWallLayoutPermissions = ModifyLayoutPermission,

        // Media access permission for the running videowall instance.
        // PTZ is intended here - by SpaceX request.
        VideoWallMediaPermissions = ReadPermission
            | ViewContentPermission
            | ViewLivePermission
            | ViewFootagePermission
            | WritePtzPermission,

        //-----------------------------------------------------------------------------------------

        // All eventually possible permissions.
        AllPermissions = 0xFFFFFFFF
    };

    Q_DECLARE_FLAGS(Permissions, Permission)
    Q_DECLARE_OPERATORS_FOR_FLAGS(Permissions)
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Permission)

    /**
     * An enumeration for user role types: predefined roles, custom groups, custom permissions.
     */
    enum class UserRole
    {
        customUserRole = -2,
        customPermissions = -1,
        owner = 0,
        administrator,
        advancedViewer,
        viewer,
        liveViewer
    };

    enum ConnectionResult
    {
        SuccessConnectionResult,                    /*< Connection available. */
        NetworkErrorConnectionResult,               /*< Connection could not be established. */
        UnauthorizedConnectionResult,               /*< Invalid login/password. */
        LdapTemporaryUnauthorizedConnectionResult,  /*< LDAP server is not accessible. */
        CloudTemporaryUnauthorizedConnectionResult, /*< CLOUD server is not accessible. */
        IncompatibleInternalConnectionResult,       /*< Server has incompatible customization. */
        IncompatibleCloudHostConnectionResult,      /*< Server has different cloud host. */
        IncompatibleVersionConnectionResult,        /*< Server version is too low. */
        IncompatibleProtocolConnectionResult,       /*< Ec2 protocol versions differs.*/
        ForbiddenConnectionResult,                  /*< Connection is not allowed yet. Try again later*/
        DisabledUserConnectionResult,               /*< Disabled user*/
        UserTemporaryLockedOut,                     /*< User is prohibited from logging in for several minutes. Try again later*/
    };

    enum MediaStreamEvent
    {
        NoEvent,

        TooManyOpenedConnections,
        ForbiddenWithDefaultPassword,
        ForbiddenWithNoLicense,
        oldFirmware,
    };
    QString toString(MediaStreamEvent value);

    enum class StreamIndex
    {
        undefined = -1,
        primary = 0,
        secondary = 1
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(StreamIndex)
    QString toString(StreamIndex value);

    enum StorageStatus
    {
        none = 0,
        used = 1 << 1,
        tooSmall = 1 << 2,
        system = 1 << 3,
        removable = 1 << 4,
        beingChecked = 1 << 5,
        beingRebuilt = 1 << 6
    };
    Q_DECLARE_FLAGS(StorageStatuses, StorageStatus)
    Q_DECLARE_OPERATORS_FOR_FLAGS(StorageStatuses)
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(StorageStatus)

    /**
     * Invalid value for a timezone UTC offset.
     */
    static const qint64 InvalidUtcOffset = std::numeric_limits<qint64>::max();
#define InvalidUtcOffset InvalidUtcOffset

    /**
     * Helper function that can be used to 'place' macros into Qn namespace.
     */
    template<class T>
    const T &_id(const T &value) { return value; }

    const static QLatin1String kWallpapersFolder("wallpapers");

} // namespace Qn

using nx::vms::api::GlobalPermission;
using nx::vms::api::GlobalPermissions;

Q_DECLARE_METATYPE(Qn::ResourceFlags)

// TODO: #Elric #enum

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::TimePeriodContent)(Qn::UserRole)(Qn::ConnectionResult),
    (metatype)
)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::PtzObjectType)(Qn::PtzCommand)(Qn::PtzCoordinateSpace)
    (Qn::StatisticsDeviceType)
    (Qn::StorageInitResult)
    (Qn::PanicMode)
    (Qn::ResourceStatus)(Qn::StatusChangeReason)
    (Qn::ConnectionRole)
    (Qn::RebuildState)(Qn::BackupState)
    (Qn::BookmarkSortField)(Qt::SortOrder)
    (Qn::RebuildAction)(Qn::BackupAction)
    (Qn::TTHeaderFlag)(Qn::IOPortType)(Qn::IODefaultState)(Qn::AuditRecordType)(Qn::AuthResult)
    (Qn::MediaStreamEvent)(Qn::StreamIndex)
    ,
    (metatype)(lexical)
)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::IOPortTypes)(Qn::Permission)(Qn::Permissions)(Qn::StorageStatus)(Qn::StorageStatuses),
    (metatype)(numeric)(lexical)
)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::PtzDataFields)(Qn::TTHeaderFlags)(Qn::ResourceInfoLevel),
    (metatype)(numeric)
)
