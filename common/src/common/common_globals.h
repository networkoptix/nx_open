#pragma once

#include <limits>

#include <QtCore/QtGlobal>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/serialization_format.h>

#ifdef THIS_BLOCK_IS_REQUIRED_TO_MAKE_FILE_BE_PROCESSED_BY_MOC_DO_NOT_DELETE
Q_OBJECT
#endif
QN_DECLARE_METAOBJECT_HEADER(Qn,
    ExtrapolationMode CameraCapability PtzObjectType PtzCommand PtzDataField PtzCoordinateSpace
    StreamFpsSharingMethod MotionType TimePeriodContent SystemComponent
    ConnectionRole ResourceStatus BitratePerGopType
    StreamQuality SecondStreamQuality PanicMode RebuildState BackupState RecordingType PeerType StatisticsDeviceType
    ServerFlag BackupType StorageInitResult CameraBackupQuality CameraStatusFlag IOPortType IODefaultState AuditRecordType AuthResult
    RebuildAction BackupAction FailoverPriority
    Permission GlobalPermission UserRole ConnectionResult
    ,
    Borders Corners ResourceFlags CameraCapabilities PtzDataFields
    MotionTypes
    ServerFlags CameraBackupQualities TimeFlags CameraStatusFlags IOPortTypes
    Permissions GlobalPermissions
    )

    enum ExtrapolationMode {
        ConstantExtrapolation,
        LinearExtrapolation,
        PeriodicExtrapolation
    };

    enum CameraCapability {
        NoCapabilities                      = 0x000,
        PrimaryStreamSoftMotionCapability   = 0x004,
        RelayInputCapability                = 0x008,
        RelayOutputCapability               = 0x010,
        ShareIpCapability                   = 0x020,
        AudioTransmitCapability             = 0x040,
        RemoteArchiveCapability             = 0x100
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

        GetAuxilaryTraitsPtzCommand,
        RunAuxilaryCommandPtzCommand,

        GetDataPtzCommand,

        InvalidPtzCommand = -1
    };

    enum PtzDataField {
        CapabilitiesPtzField    = 0x001,
        DevicePositionPtzField  = 0x002,
        LogicalPositionPtzField = 0x004,
        DeviceLimitsPtzField    = 0x008,
        LogicalLimitsPtzField   = 0x010,
        FlipPtzField            = 0x020,
        PresetsPtzField         = 0x040,
        ToursPtzField           = 0x080,
        ActiveObjectPtzField    = 0x100,
        HomeObjectPtzField      = 0x200,
        AuxilaryTraitsPtzField  = 0x400,
        NoPtzFields             = 0x000,
        AllPtzFields            = 0xFFF
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


    enum MotionType {
        MT_Default      = 0x0,
        MT_HardwareGrid = 0x1,
        MT_SoftwareGrid = 0x2,
        MT_MotionWindow = 0x4,
        MT_NoMotion     = 0x8
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(MotionType)

    Q_DECLARE_FLAGS(MotionTypes, MotionType)
    Q_DECLARE_OPERATORS_FOR_FLAGS(MotionTypes)


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
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResourceStatus)

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

    enum BitratePerGopType {
        BPG_None,
        BPG_Predefined,
        BPG_User
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(BitratePerGopType)

    // TODO: #Elric #EC2 talk to Roma, write comments
    enum ServerFlag {
        SF_None = 0x000,
        SF_Edge = 0x001,
        SF_RemoteEC = 0x002,
        SF_HasPublicIP = 0x004,
        SF_IfListCtrl = 0x008,
        SF_timeCtrl = 0x010,
        //SF_AutoSystemName = 0x020, /**< System name is default, so it will be displayed as "Unassigned System' in NxTool. */
        SF_ArmServer = 0x040,
        SF_Has_HDD = 0x080,
        SF_NewSystem = 0x100, /**< System is just installed, it has default admin password and is not linked to the cloud. */
        SF_SupportsTranscoding = 0x200,
        SF_HasLiteClient = 0x400,
        SF_P2pSyncDone = 0x1000000, //< For UT purpose only
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ServerFlag)

    Q_DECLARE_FLAGS(ServerFlags, ServerFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(ServerFlags)


    enum TimeFlag
    {
        TF_none = 0x0,
        TF_peerIsNotEdgeServer = 0x0001,
        TF_peerHasMonotonicClock = 0x0002,
        TF_peerTimeSetByUser = 0x0004,
        TF_peerTimeSynchronizedWithInternetServer = 0x0008,
        TF_peerIsServer = 0x1000
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(TimeFlag)

    Q_DECLARE_FLAGS(TimeFlags, TimeFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(TimeFlags)


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
        AR_CameraInsert      = 0x80000
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

        TimePeriodContentCount
    };

    enum SystemComponent {
        ServerComponent,
        ClientComponent,
        AnyComponent
    };

    // TODO: #Elric #EC2 rename
    enum StreamQuality {
        QualityLowest = 0,
        QualityLow = 1,
        QualityNormal = 2,
        QualityHigh = 3,
        QualityHighest = 4,
        QualityPreSet = 5,
        QualityNotDefined = 6,

        StreamQualityCount
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(StreamQuality)


    // TODO: #Elric #EC2 rename
    enum SecondStreamQuality {
        SSQualityLow = 0,
        SSQualityMedium = 1,
        SSQualityHigh = 2,
        SSQualityNotDefined = 3,
        SSQualityDontUse = 4
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SecondStreamQuality)

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


    enum CameraStatusFlag {
        CSF_NoFlags = 0x0,
        CSF_HasIssuesFlag = 0x1
    };
    Q_DECLARE_FLAGS(CameraStatusFlags, CameraStatusFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(CameraStatusFlags)
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(CameraStatusFlag)

    // TODO: #Elric #EC2 rename
    enum RecordingType {
        /** Record always. */
        RT_Always = 0,

        /** Record only when motion was detected. */
        RT_MotionOnly = 1,

        /** Don't record. */
        RT_Never = 2,

        /** Record LQ stream always and HQ stream only on motion. */
        RT_MotionAndLowQuality = 3,

        RT_Count
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(RecordingType)

    enum PeerType {
        PT_NotDefined = -1,
        PT_Server = 0,
        PT_DesktopClient = 1,
        PT_VideowallClient = 2,
        PT_OldMobileClient = 3,
        PT_MobileClient = 4,
        PT_CloudServer = 5,
        PT_OldServer = 6, //< 2.6 or below
        PT_Count
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PeerType)

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
        BookmarkCameraName      //< Sorted manually!
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
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(AuthResult)
    QString toString(AuthResult value);


    enum FailoverPriority
    {
        FP_Never,
        FP_Low,
        FP_Medium,
        FP_High,

        FP_Count
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(FailoverPriority)
    static_assert(FP_Medium == 2, "Value is hardcoded in SQL migration script.");

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
    enum BackupType
    {
        Backup_Manual,
        Backup_RealTime,
        Backup_Schedule
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(BackupType)

    enum CameraBackupQuality
    {
        CameraBackup_Disabled       = 0,
        CameraBackup_HighQuality    = 1,
        CameraBackup_LowQuality     = 2,
        CameraBackup_Both           = CameraBackup_HighQuality | CameraBackup_LowQuality,
        CameraBackup_Default        = 4 // backup type didn't configured so far. Default value will be used
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(CameraBackupQuality)
    Q_DECLARE_FLAGS(CameraBackupQualities, CameraBackupQuality)
    Q_DECLARE_OPERATORS_FOR_FLAGS(CameraBackupQualities)

    enum StorageInitResult
    {
        StorageInit_Ok,
        StorageInit_CreateFailed,
        StorageInit_WrongPath,
        StorageInit_WrongAuth,
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(StorageInitResult)

    /**
     * Flags describing the actions permitted for the user to do with the
     * selected resource. Calculated in runtime.
     */
    enum Permission
    {
        /* Generic permissions. */
        NoPermissions                   = 0x0000,   /**< No access */

        ReadPermission                  = 0x0001,   /**< Generic read access. Having this access right doesn't necessary mean that all information is readable. */
        WritePermission                 = 0x0002,   /**< Generic write access. Having this access right doesn't necessary mean that all information is writable. */
        SavePermission                  = 0x0004,   /**< Generic save access. Entity can be saved to the server. */
        RemovePermission                = 0x0008,   /**< Generic delete permission. */
        ReadWriteSavePermission = ReadPermission | WritePermission | SavePermission,
        WriteNamePermission             = 0x0010,   /**< Permission to edit resource's name. */

        /**
         * Permission to view resource content.
         * Currently used for server's health monitor access.
         * Automatically granted for cameras and web pages if user has ReadPermission for them.
         */
        ViewContentPermission           = 0x0020,

        /** Full set of permissions which can be available for server resource. */
        FullServerPermissions           = ReadWriteSavePermission | WriteNamePermission | RemovePermission | ViewContentPermission,

        /* Layout-specific permissions. */
        AddRemoveItemsPermission        = 0x0040,   /**< Permission to add or remove items from a layout. */
        EditLayoutSettingsPermission    = 0x0080,   /**< Permission to setup layout background or set locked flag. */
        ModifyLayoutPermission          = ReadPermission | WritePermission | AddRemoveItemsPermission, /**< Permission to modify without saving. */
        FullLayoutPermissions           = ReadWriteSavePermission | WriteNamePermission | RemovePermission | ModifyLayoutPermission | EditLayoutSettingsPermission,

        /* User-specific permissions. */
        WritePasswordPermission         = 0x0200,   /**< Permission to edit associated password. */
        WriteAccessRightsPermission     = 0x0400,   /**< Permission to edit access rights. */
        WriteEmailPermission            = 0x0800,   /**< Permission to edit user's email. */
        WriteFullNamePermission         = 0x1000,   /**< Permission to edit user's full name. */
        FullUserPermissions             = ReadWriteSavePermission | WriteNamePermission
                                            | RemovePermission | WritePasswordPermission
                                            | WriteAccessRightsPermission
                                            | WriteFullNamePermission | WriteEmailPermission,

        /* Media-specific permissions. */
        ExportPermission                = 0x2000,   /**< Permission to export video parts. */

        /* Camera-specific permissions. */
        WritePtzPermission              = 0x4000,   /**< Permission to use camera's PTZ controls. */

        /* Mode-specific permissions. */
        VideoWallLayoutPermissions      = ModifyLayoutPermission,
        VideoWallMediaPermissions       = ReadPermission | ViewContentPermission | WritePtzPermission,

        AllPermissions = 0xFFFFFFFF
    };

    Q_DECLARE_FLAGS(Permissions, Permission)
    Q_DECLARE_OPERATORS_FOR_FLAGS(Permissions)
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Permission)



    /**
     * Flags describing global user capabilities, independently of resources. Stored in the database.
     * QFlags uses int internally, so we are limited to 32 bits.
     */
    enum GlobalPermission
    {
        /* Generic permissions. */
        NoGlobalPermissions                     = 0x00000000,   /**< Only live video access. */

        /* Admin permissions. */
        GlobalAdminPermission                   = 0x00000001,   /**< Admin, can edit other non-admins. */

        /* Manager permissions. */
        GlobalEditCamerasPermission             = 0x00000002,   /**< Can edit camera settings. */
        GlobalControlVideoWallPermission        = 0x00000004,   /**< Can control videowalls. */

        GlobalViewLogsPermission                = 0x00000010,   /**< Can access event log and audit trail. */

        /* Viewer permissions. */
        GlobalViewArchivePermission             = 0x00000100,   /**< Can view archives of available cameras. */
        GlobalExportPermission                  = 0x00000200,   /**< Can export archives of available cameras. */
        GlobalViewBookmarksPermission           = 0x00000400,   /**< Can view bookmarks of available cameras. */
        GlobalManageBookmarksPermission         = 0x00000800,   /**< Can modify bookmarks of available cameras. */

        /* Input permissions. */
        GlobalUserInputPermission               = 0x00010000,   /**< Can change camera's PTZ state, use 2-way audio, I/O buttons. */

        /* Resources access permissions */
        GlobalAccessAllMediaPermission          = 0x01000000,   /**< Has access to all media resources (cameras and web pages). */


        GlobalCustomUserPermission              = 0x10000000,   /**< Flag that just mark new user as 'custom'. */

        /* Shortcuts. */

        /* Live viewer has access to all cameras and global layouts by default. */
        GlobalLiveViewerPermissionSet       = GlobalAccessAllMediaPermission,

        /* Viewer can additionally view archive and bookmarks and export video. */
        GlobalViewerPermissionSet           = GlobalLiveViewerPermissionSet | GlobalViewArchivePermission | GlobalExportPermission | GlobalViewBookmarksPermission,

        /* Advanced viewer can manage bookmarks and use various input methods. */
        GlobalAdvancedViewerPermissionSet   = GlobalViewerPermissionSet | GlobalManageBookmarksPermission | GlobalUserInputPermission | GlobalViewLogsPermission,

        /* Admin can do everything. */
        GlobalAdminPermissionSet            = GlobalAdminPermission | GlobalAdvancedViewerPermissionSet | GlobalControlVideoWallPermission | GlobalEditCamerasPermission,

        /* PTZ here is intended - for SpaceX, see VMS-2208 */
        GlobalVideoWallModePermissionSet    = GlobalLiveViewerPermissionSet | GlobalViewArchivePermission | GlobalUserInputPermission |
                                              GlobalControlVideoWallPermission | GlobalViewBookmarksPermission,

        /* Actions in ActiveX plugin mode are limited. */
        GlobalActiveXModePermissionSet      = GlobalViewerPermissionSet | GlobalUserInputPermission,
    };

    Q_DECLARE_FLAGS(GlobalPermissions, GlobalPermission)
    Q_DECLARE_OPERATORS_FOR_FLAGS(GlobalPermissions)
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(GlobalPermission)


    /**
    * An enumeration for user role types: predefined roles, custom groups, custom permissions.
    */
    enum class UserRole
    {
        CustomUserRole = -2,
        CustomPermissions = -1,
        Owner = 0,
        Administrator,
        AdvancedViewer,
        Viewer,
        LiveViewer,
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
        ForbiddenConnectionResult,                   /*< Connection is not allowed yet. Try again later*/
        DisabledUserConnectionResult                /*< Disabled user*/
    };

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

Q_DECLARE_METATYPE(Qn::StatusChangeReason)
Q_DECLARE_METATYPE(Qn::ResourceFlags)

// TODO: #Elric #enum

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::TimePeriodContent)(Qn::UserRole)(Qn::ConnectionResult),
    (metatype)
)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::PtzObjectType)(Qn::PtzCommand)(Qn::PtzCoordinateSpace)(Qn::MotionType)
    (Qn::StreamQuality)(Qn::SecondStreamQuality)(Qn::StatisticsDeviceType)
    (Qn::ServerFlag)(Qn::BackupType)(Qn::CameraBackupQuality)(Qn::StorageInitResult)
    (Qn::PanicMode)(Qn::RecordingType)
    (Qn::ConnectionRole)(Qn::ResourceStatus)(Qn::BitratePerGopType)
    (Qn::PeerType)(Qn::RebuildState)(Qn::BackupState)
    (Qn::BookmarkSortField)(Qt::SortOrder)
    (Qn::RebuildAction)(Qn::BackupAction)
    (Qn::TTHeaderFlag)(Qn::IOPortType)(Qn::IODefaultState)(Qn::AuditRecordType)(Qn::AuthResult)
    (Qn::FailoverPriority)
    ,
    (metatype)(lexical)
)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::ServerFlags)(Qn::CameraBackupQualities)(Qn::TimeFlags)(Qn::CameraStatusFlags)
    (Qn::Permission)(Qn::GlobalPermission)(Qn::Permissions)(Qn::GlobalPermissions)(Qn::IOPortTypes)
    ,
    (metatype)(numeric)(lexical)
)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::PtzDataFields)(Qn::TTHeaderFlags)(Qn::ResourceInfoLevel),
    (metatype)(numeric)
)
