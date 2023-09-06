// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ostream>

#include <QtCore/QMetaType>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/serialization/flags.h>

namespace nx::vms::api {

Q_NAMESPACE_EXPORT(NX_VMS_API)
Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

enum class ResourceStatus
{
    /**%apidoc The Device is inaccessible.
     */
    offline = 0,

    /**%apidoc The Device does not have correct credentials in the database.
     */
    unauthorized = 1,

    /**%apidoc The Device is operating normally.
     */
    online = 2,

    /**%apidoc Applies only to Cameras. The Camera is online and recording the video stream.
     */
    recording = 3,

    /**%apidoc
     * The Device status is unknown. It may show up while Servers synchronize status information.
     * If this status persists, it indicates an internal System problem.
     */
    undefined = 4,

    /**%apidoc
     * Applies only to Servers. The Server is incompatible only when it has the System name
     * different from the current one, or it has an incompatible protocol version.
     */
    incompatible = 5,

    /**%apidoc
     * Applies only to Servers. Server is in this state if its DB certificate doesn't match the
     * certificate provided during the SSL handshake on a Server-to-Server connection. This may
     * happen in the case of a man-in-the-middle attack, or in the case of a certificate loss on
     * the target Server.
     */
    mismatchedCertificate = 6,
};
Q_ENUM_NS(ResourceStatus)

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(ResourceStatus*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<ResourceStatus>;
    return visitor(
        Item{ResourceStatus::offline, "Offline"},
        Item{ResourceStatus::unauthorized, "Unauthorized"},
        Item{ResourceStatus::online, "Online"},
        Item{ResourceStatus::recording, "Recording"},
        Item{ResourceStatus::undefined, "NotDefined"},
        Item{ResourceStatus::incompatible, "Incompatible"},
        Item{ResourceStatus::mismatchedCertificate, "mismatchedCertificate"}
    );
}

inline uint qHash(ResourceStatus value, uint seed = 0)
{
    return QT_PREPEND_NAMESPACE(qHash)((int) value, seed);
}

NX_REFLECTION_ENUM(CameraStatusFlag,
    CSF_NoFlags = 0,
    CSF_HasIssuesFlag = 1 << 0,
    CSF_InvalidScheduleFlag = 1 << 1
)
Q_DECLARE_FLAGS(CameraStatusFlags, CameraStatusFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(CameraStatusFlags)

/**%apidoc When to record. */
enum class RecordingType
{
    always = 0, /**<%apidoc Record always. */

    metadataOnly = 1, /**<%apidoc Record only when motion was detected. */

    never = 2, /**<%apidoc Don't record. */

    metadataAndLowQuality = 3, /**<%apidoc Record LQ stream always and HQ stream only on motion. */
};

// Compatibility layer.
template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(RecordingType*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<RecordingType>;
    return visitor(
        Item{RecordingType::always, "always"},
        Item{RecordingType::always, "RT_Always"},
        Item{RecordingType::metadataOnly, "metadataOnly"},
        Item{RecordingType::metadataOnly, "RT_MotionOnly"},
        Item{RecordingType::never, "never"},
        Item{RecordingType::never, "RT_Never"},
        Item{RecordingType::metadataAndLowQuality, "metadataAndLowQuality"},
        Item{RecordingType::metadataAndLowQuality, "RT_MotionAndLowQuality"}
    );
}

NX_VMS_API std::ostream& operator<<(std::ostream& os, RecordingType value);

enum class StreamQuality
{
    lowest = 0,
    low = 1,
    normal = 2,
    high = 3,
    highest = 4,
    preset = 5,
    undefined = 6,

    /**%apidoc[unused]
     * %// Used for rapid review only. The bitrate should be very high.
     */
    rapidReview = 7,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(StreamQuality*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<StreamQuality>;
    return visitor(
        Item{StreamQuality::lowest, "lowest"},
        Item{StreamQuality::low, "low"},
        Item{StreamQuality::normal, "normal"},
        Item{StreamQuality::high, "high"},
        Item{StreamQuality::highest, "highest"},
        Item{StreamQuality::preset, "present"},
        Item{StreamQuality::undefined, "undefined"},
        Item{StreamQuality::undefined, ""},
        Item{StreamQuality::rapidReview, "rapidPreview"}
    );
}

NX_VMS_API std::ostream& operator<<(std::ostream& os, StreamQuality value);

NX_REFLECTION_ENUM_CLASS(RecordingMetadataType,
    none = 0,
    motion = 1 << 0,
    objects = 1 << 1
)
Q_DECLARE_FLAGS(RecordingMetadataTypes, RecordingMetadataType)
Q_DECLARE_OPERATORS_FOR_FLAGS(RecordingMetadataTypes)

NX_VMS_API std::ostream& operator<<(std::ostream& os, RecordingMetadataType value);
NX_VMS_API std::ostream& operator<<(std::ostream& os, RecordingMetadataTypes value);

/**%apidoc
 * Priority for the Camera to be moved to another Server for failover (if the current Server
 * fails).
 */
enum class FailoverPriority
{
    /**%apidoc Will never be moved to another Server.
     * %caption Never
     */
    never = 0,

    /**%apidoc Low priority against other Cameras.
     * %caption Low
     */
    low = 1,

    /**%apidoc Medium priority against other Cameras.
     * %caption Medium
     */
    medium = 2,

    /**%apidoc High priority against other cameras.
     * %caption High
     */
    high = 3,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(FailoverPriority*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<FailoverPriority>;
    return visitor(
        Item{FailoverPriority::never, "Never"},
        Item{FailoverPriority::low, "Low"},
        Item{FailoverPriority::medium, "Medium"},
        Item{FailoverPriority::high, "High"}
    );
}

enum CameraBackupQuality
{
    CameraBackupBoth = 0,
    CameraBackupHighQuality = 1,
    CameraBackupLowQuality = 2,
    CameraBackupDefault = 3, //< Backup quality is not configured yet; the default will be used.
};

inline bool isHiQualityEnabled(const nx::vms::api::CameraBackupQuality& quality)
{
    return quality == nx::vms::api::CameraBackupQuality::CameraBackupHighQuality
        || quality == nx::vms::api::CameraBackupQuality::CameraBackupBoth;
}

inline bool isLowQualityEnabled(const nx::vms::api::CameraBackupQuality& quality)
{
    return quality == nx::vms::api::CameraBackupQuality::CameraBackupLowQuality
        || quality == nx::vms::api::CameraBackupQuality::CameraBackupBoth;
}

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(CameraBackupQuality*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<CameraBackupQuality>;
    return visitor(
        Item{ CameraBackupHighQuality, "CameraBackupHighQuality" },
        Item{ CameraBackupLowQuality, "CameraBackupLowQuality" },
        Item{ CameraBackupBoth, "CameraBackupBoth" },
        Item{ CameraBackupDefault, "CameraBackupDefault" }
    );
}

// TODO: #rvasilenko Write comments.
/**
 * ATTENTION: When removing unneeded Server Flags, their constants must be renamed to
 * SF_Obsolete_... instead of being deleted, and a manual deserialization code must be added to_char_type
 * be able to deserialize the flag from its original name - this is needed to make new Clients,
 * including a Mobile Client, compatible with the old Servers which may produce these flags.
 */
enum ServerFlag
{
    SF_None = 0x000,
    SF_Edge = 0x001,
    SF_RemoteEC = 0x002,
    SF_HasPublicIP = 0x004,
    SF_IfListCtrl = 0x008,
    SF_timeCtrl = 0x010,

    /**%apidoc[unused]
     * System name is default, so it will be displayed as "Unassigned System' in NxTool.
     */
    SF_Obsolete_AutoSystemName = 0x020,

    /**%apidoc The Server has a setting isArmServer=true. */
    SF_ArmServer = 0x040,

    /**%apidoc[unused] Removed in 5.1, was proprietary. */
    SF_Obsolete_Has_HDD = 0x080,

    /**%apidoc
     * The System is just installed, it has the default admin password and is not connected to the
     * Cloud.
     * %deprecated Is returned by the API only for compatibility. A proper indicator of a new
     *     System is the empty (null) localSystemId.
     */
    SF_NewSystem = 0x100,

    SF_SupportsTranscoding = 0x200,

    /**%apidoc[unused] For unit test purpose only. */
    SF_P2pSyncDone = 0x1000000,

    /**%apidoc[unused] Removed in 5.1 - Edge-only licenses are gone. */
    SF_Obsolete_RequiresEdgeLicense = 0x2000000,

    /**%apidoc Server can provide information about built-in PoE block. */
    SF_HasPoeManagementCapability = 0x4000000,

    /**%apidoc Server can provide information about internal fan and its state. */
    SF_HasFanMonitoringCapability = 0x8000000,

    /**%apidoc Server has a buzzer that can produce sound. */
    SF_HasBuzzer = 0x10000000,

    SF_OwnerApiForAdmins = 0x20000000,
};
Q_DECLARE_FLAGS(ServerFlags, ServerFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(ServerFlags)

/** NOTE: Explicit deserialization is needed to enable obsolete Server Flags. */
template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(ServerFlag*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<ServerFlag>;
    return visitor(
        Item{ServerFlag::SF_None, "SF_None"},
        Item{ServerFlag::SF_Edge, "SF_Edge"},
        Item{ServerFlag::SF_RemoteEC, "SF_RemoteEC"},
        Item{ServerFlag::SF_HasPublicIP, "SF_HasPublicIP"},
        Item{ServerFlag::SF_IfListCtrl, "SF_IfListCtrl"},
        Item{ServerFlag::SF_timeCtrl, "SF_timeCtrl"},
        Item{ServerFlag::SF_Obsolete_AutoSystemName, "SF_AutoSystemName"},
        Item{ServerFlag::SF_ArmServer, "SF_ArmServer"},
        Item{ServerFlag::SF_Obsolete_Has_HDD, "SF_Has_HDD"},
        Item{ServerFlag::SF_NewSystem, "SF_NewSystem"},
        Item{ServerFlag::SF_SupportsTranscoding, "SF_SupportsTranscoding"},
        Item{ServerFlag::SF_P2pSyncDone, "SF_P2pSyncDone"},
        Item{ServerFlag::SF_Obsolete_RequiresEdgeLicense, "SF_RequiresEdgeLicense"},
        Item{ServerFlag::SF_HasPoeManagementCapability, "SF_HasPoeManagementCapability"},
        Item{ServerFlag::SF_HasFanMonitoringCapability, "SF_HasFanMonitoringCapability"},
        Item{ServerFlag::SF_HasBuzzer, "SF_HasBuzzer"},
        Item{ServerFlag::SF_OwnerApiForAdmins, "SF_OwnerApiForAdmins"}
    );
}

enum class IoModuleVisualStyle
{
    form,
    tile
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(IoModuleVisualStyle*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<IoModuleVisualStyle>;
    return visitor(
        Item{IoModuleVisualStyle::form, "Form"},
        Item{IoModuleVisualStyle::tile, "Tile"}
    );
}

NX_REFLECTION_ENUM_CLASS(StreamDataFilter,
    media = 1 << 0, //< Send media data.
    motion = 1 << 1, //< Send motion data.
    objects = 1 << 2 //< Send analytics objects.
)

Q_DECLARE_FLAGS(StreamDataFilters, StreamDataFilter)
Q_DECLARE_OPERATORS_FOR_FLAGS(StreamDataFilters)

NX_REFLECTION_ENUM_CLASS(MetadataStorageChangePolicy,
    keep,
    remove,
    move
)

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::FailoverPriority)
Q_DECLARE_METATYPE(nx::vms::api::CameraBackupQuality)
Q_DECLARE_METATYPE(nx::vms::api::CameraStatusFlag)
Q_DECLARE_METATYPE(nx::vms::api::CameraStatusFlags)
Q_DECLARE_METATYPE(nx::vms::api::MetadataStorageChangePolicy)
Q_DECLARE_METATYPE(nx::vms::api::RecordingMetadataType)
Q_DECLARE_METATYPE(nx::vms::api::RecordingMetadataTypes)
Q_DECLARE_METATYPE(nx::vms::api::ResourceStatus)
Q_DECLARE_METATYPE(nx::vms::api::ServerFlag)
Q_DECLARE_METATYPE(nx::vms::api::ServerFlags)
Q_DECLARE_METATYPE(nx::vms::api::StreamDataFilter)
Q_DECLARE_METATYPE(nx::vms::api::StreamDataFilters)
