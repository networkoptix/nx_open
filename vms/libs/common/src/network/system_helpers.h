#pragma once

#include <nx/utils/uuid.h>
#include <nx/vms/api/data_fwd.h>

#include <core/resource/resource_fwd.h>

class QUrl;

struct QnCloudSystem;
struct QnConnectionInfo;
class QnCommonModule;

namespace helpers {

static const QString kFactorySystemUser = "admin";
static const QString kFactorySystemPassword = "admin";

/*
* Extracts system id. Result is:
* - identifier of server if it is in "new state"
* - cloud id if server in cloud
* - system name in other cases
*/
QString getTargetSystemId(const QnConnectionInfo& info);
QString getTargetSystemId(const nx::vms::api::ModuleInformation& info);
QString getTargetSystemId(const QnCloudSystem& cloudSystem);

QnUuid getLocalSystemId(const nx::vms::api::ModuleInformation& info);
QnUuid getLocalSystemId(const QnConnectionInfo& info);

QString getFactorySystemId(const nx::vms::api::ModuleInformation& info);

bool isSafeMode(const nx::vms::api::ModuleInformation& info);

bool isNewSystem(const QnConnectionInfo& info);
bool isNewSystem(const nx::vms::api::ModuleInformation& info);
bool isNewSystem(const QnCloudSystem& info);

bool isCloudSystem(const nx::vms::api::ModuleInformation& info);

QnUuid currentSystemLocalId(const QnCommonModule* commonModule);

bool currentSystemIsNew(const QnCommonModule* commonModule);

bool serverBelongsToCurrentSystem(const nx::vms::api::ModuleInformation& info,
    const QnCommonModule* commonModule);

bool serverBelongsToCurrentSystem(const QnMediaServerResourcePtr& server);

/**
 * Checks whether user login is local or cloud.
 * We suppose local login does not have '@' symbol whereas cloud does
 */
bool isLocalUser(const QString& login);
} // helpers
