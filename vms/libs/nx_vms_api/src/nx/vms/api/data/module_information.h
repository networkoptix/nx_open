// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QSet>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/os_info.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/saas_data.h>
#include <nx/vms/api/types/resource_types.h>

#include "data_macros.h"
#include "os_information.h"
#include "runtime_data.h"
#include "server_timezone_information.h"
#include "software_version_serialization.h"
#include "timestamp.h"

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(HwPlatform,
    unknown = 0,
    raspberryPi,
    bananaPi
)

struct NX_VMS_API ServerPortInformation
{
    int port = 0;
    QnUuid id;
    bool operator==(const ServerPortInformation& other) const = default;
};
#define ServerPortInformation_Fields (port)(id)
NX_VMS_API_DECLARE_STRUCT_EX(ServerPortInformation, (ubjson)(json)(xml)(csv_record))
NX_REFLECTION_INSTRUMENT(ServerPortInformation, ServerPortInformation_Fields);

struct NX_VMS_API ModuleInformation: ServerPortInformation
{
    QString type;
    QString customization;
    QString brand;
    nx::utils::SoftwareVersion version; /**<%apidoc:string */
    QString systemName;
    QString name;
    bool sslAllowed = true;
    int protoVersion = 0;
    QnUuid runtimeId;
    ServerFlags serverFlags = {};
    QString realm;
    bool ecDbReadOnly = false;
    QString cloudSystemId;
    QString cloudPortalUrl;
    QString cloudHost;
    QnUuid localSystemId;
    HwPlatform hwPlatform;

    /**%apidoc Current time synchronized with the VMS System, in milliseconds since epoch. */
    std::chrono::milliseconds synchronizedTimeMs{0};

    /**%apidoc Presented if the System is bound to the Cloud. */
    std::optional<QnUuid> cloudOwnerId;
    std::optional<QnUuid> organizationId;
    nx::vms::api::SaasState saasState = nx::vms::api::SaasState::uninitialized;

    void fixRuntimeId();
    QString cloudId() const;
    bool isNewSystem() const;

    bool operator==(const ModuleInformation& other) const = default;

    static QString mediaServerId();
    static QString clientId();
};

#define ModuleInformation_Fields \
    (type) \
    (customization) \
    (version) \
    (systemName) \
    (name) \
    (port) \
    (id) \
    (sslAllowed) \
    (protoVersion) \
    (runtimeId) \
    (serverFlags) \
    (realm) \
    (ecDbReadOnly) \
    (cloudSystemId) \
    (cloudHost) \
    (brand) \
    (localSystemId) \
    (hwPlatform) \
    (synchronizedTimeMs) \
    (cloudOwnerId) \
    (organizationId) \
    (saasState)
NX_VMS_API_DECLARE_STRUCT_EX(ModuleInformation, (ubjson)(json)(xml)(csv_record))
NX_REFLECTION_INSTRUMENT(ModuleInformation, ModuleInformation_Fields);

struct NX_VMS_API ModuleInformationWithAddresses: ModuleInformation
{
    /**%apidoc:stringArray
     * If any of these addresses don't contain port, ModuleInformation::port must be used.
     */
    QSet<QString> remoteAddresses;

    ModuleInformationWithAddresses() = default;
    ModuleInformationWithAddresses& operator=(const ModuleInformationWithAddresses&) = default;
    ModuleInformationWithAddresses(const ModuleInformationWithAddresses& other) = default;
    ModuleInformationWithAddresses(const ModuleInformation& other): ModuleInformation(other) {}

    bool operator==(const ModuleInformationWithAddresses& other) const = default;
};

#define ModuleInformationWithAddresses_Fields ModuleInformation_Fields (remoteAddresses)
NX_VMS_API_DECLARE_STRUCT_EX(ModuleInformationWithAddresses, (ubjson)(json)(xml)(csv_record))
NX_REFLECTION_INSTRUMENT(ModuleInformationWithAddresses, ModuleInformationWithAddresses_Fields);

struct NX_VMS_API TransactionLogTime
{
    int64_t sequence{0};
    std::chrono::milliseconds ticksMs{0};

    TransactionLogTime(Timestamp t = {}): sequence(t.sequence), ticksMs(t.ticks) {}
    operator Timestamp() const { return Timestamp(sequence, ticksMs.count()); }

    bool operator==(const TransactionLogTime& other) const = default;
};

#define TransactionLogTime_Fields (sequence)(ticksMs)
NX_VMS_API_DECLARE_STRUCT_EX(TransactionLogTime, (json))
NX_REFLECTION_INSTRUMENT(TransactionLogTime, TransactionLogTime_Fields);

struct NX_VMS_API ServerInformation: ModuleInformationWithAddresses
{
    /**%apidoc
     * The content of a user-provided file `cert.pem` on the Server file system in PEM format
     * without private key. Empty if not provided or can't be loaded and parsed. It is used by
     * the Server if no Server-specific SNI is required by a Client.
     */
    std::string userProvidedCertificatePem;

    /**%apidoc
     * Server-generated self-signed certificate in PEM format without private key. It is used by
     * the Server if Server-specific SNI is required by a Client or there is no the user-provided
     * certificate or the user-provided certificate is invalid.
     */
    std::string certificatePem;

    std::chrono::milliseconds systemIdentityTimeMs{0};
    TransactionLogTime transactionLogTime;

    bool collectedByThisServer = false;

    ServerInformation() = default;
    ServerInformation(const ServerInformation& rhs) = default;
    ServerInformation(const ModuleInformationWithAddresses& rhs): ModuleInformationWithAddresses(rhs) {}

    QnUuid getId() const { return id; }
};

#define ServerInformation_Fields ModuleInformationWithAddresses_Fields \
    (userProvidedCertificatePem) \
    (certificatePem) \
    (systemIdentityTimeMs) \
    (transactionLogTime) \
    (collectedByThisServer)

NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(ServerInformation, (json))
NX_REFLECTION_INSTRUMENT(ServerInformation, ServerInformation_Fields);

struct NX_VMS_API ServerRuntimeInformation: ServerPortInformation, ServerTimeZoneInformation
{
    nx::utils::OsInfo osInfo;

    /**%apidoc Local OS time on the Server, in milliseconds since epoch. */
    std::chrono::milliseconds osTimeMs{0};

    /**%apidoc Current time synchronized with the VMS System, in milliseconds since epoch. */
    std::chrono::milliseconds synchronizedTimeMs{0};

    RuntimeData runtimeData;

    ServerRuntimeInformation() = default;
    ServerRuntimeInformation(const ServerRuntimeInformation& rhs) = default;

    QnUuid getId() const { return id; }
};

#define ServerRuntimeInformation_Fields \
    ServerPortInformation_Fields \
    ServerTimeZoneInformation_Fields \
    (osInfo)(osTimeMs)(synchronizedTimeMs)(runtimeData)

NX_VMS_API_DECLARE_STRUCT_EX(ServerRuntimeInformation, (json))
NX_REFLECTION_INSTRUMENT(ServerRuntimeInformation, ServerRuntimeInformation_Fields);

} // namespace nx::vms::api

namespace nx::utils {

// TODO: Invent a better place for it.
// So far OsInfo serialization is required only for ModuleInformation.
QN_FUSION_DECLARE_FUNCTIONS(nx::utils::OsInfo, (ubjson)(json)(xml)(csv_record), NX_VMS_API)

} // namespace nx::utils
