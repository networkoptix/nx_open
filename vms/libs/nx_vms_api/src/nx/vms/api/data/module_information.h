#pragma once

#include "data.h"
#include "software_version.h"
#include "system_information.h"

#include <set>

#include <QtCore/QString>
#include <QtCore/QSet>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/resource_types.h>

namespace nx {
namespace vms {
namespace api {

enum class HwPlatform
{
    unknown = 0,
    raspberryPi,
    bananaPi
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::vms::api::HwPlatform)

struct NX_VMS_API ModuleInformation: Data
{
    QString type;
    QString customization;
    QString brand;
    SoftwareVersion version;
    SystemInformation systemInformation;
    QString systemName;
    QString name;
    int port = 0;
    QnUuid id;
    bool sslAllowed = false;
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

    void fixRuntimeId();
    QString cloudId() const;

    static QString nxMediaServerId();
    static QString nxECId();
    static QString nxClientId();
};

struct NX_VMS_API ModuleInformationWithAddresses: ModuleInformation
{
    // If any of these addresses don't contain port, ModuleInformation::port must be used.
    QSet<QString> remoteAddresses;

    ModuleInformationWithAddresses() = default;
    ModuleInformationWithAddresses(const ModuleInformation& other): ModuleInformation(other) {}
};

#define ModuleInformation_Fields \
    (type) \
    (customization) \
    (version) \
    (systemInformation) \
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
    (hwPlatform)

#define ModuleInformationWithAddresses_Fields \
    ModuleInformation_Fields \
    (remoteAddresses)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::ModuleInformation)
Q_DECLARE_METATYPE(nx::vms::api::ModuleInformationWithAddresses)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::HwPlatform, (lexical), NX_VMS_API)
