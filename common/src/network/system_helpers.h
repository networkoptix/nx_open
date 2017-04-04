#pragma once

#include <nx/utils/uuid.h>

#include <core/resource/resource_fwd.h>

class QUrl;

struct QnCloudSystem;
struct QnConnectionInfo;
struct QnModuleInformation;
class QnCommonModule;

namespace helpers {

static const QString kFactorySystemUser = lit("admin");
static const QString kFactorySystemPassword = lit("admin");

/*
* Extracts system id. Result is:
* - identifier of server if it is in "new state"
* - cloud id if server in cloud
* - system name in other cases
*/
QString getTargetSystemId(const QnConnectionInfo& info);
QString getTargetSystemId(const QnModuleInformation& info);
QString getTargetSystemId(const QnCloudSystem& cloudSystem);

QnUuid getLocalSystemId(const QnModuleInformation& info);
QnUuid getLocalSystemId(const QnConnectionInfo& info);

QString getFactorySystemId(const QnModuleInformation& info);

bool isSafeMode(const QnModuleInformation& info);

bool isNewSystem(const QnConnectionInfo& info);
bool isNewSystem(const QnModuleInformation& info);
bool isNewSystem(const QnCloudSystem& info);

bool isCloudSystem(const QnModuleInformation& info);

QnUuid currentSystemLocalId(const QnCommonModule* commonModule);

bool currentSystemIsNew(const QnCommonModule* commonModule);

bool serverBelongsToCurrentSystem(const QnModuleInformation& info, const QnCommonModule* commonModule);

bool serverBelongsToCurrentSystem(const QnMediaServerResourcePtr& server);

/**
 * Checks whether user login is local or cloud.
 * We suppose local login does not have '@' symbol whereas cloud does
 */
bool isLocalUser(const QString& login);
} // helpers
