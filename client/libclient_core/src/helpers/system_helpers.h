#pragma once

#include <nx/utils/uuid.h>
#include <client_core/local_connection_data.h>

class QUrl;

struct QnCloudSystem;
struct QnConnectionInfo;
struct QnModuleInformation;

namespace helpers {

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

bool isNewSystem(const QnModuleInformation& info);
bool isNewSystem(const QnCloudSystem& info);

QnLocalConnectionData storeLocalSystemConnection(
    const QString& systemName,
    const QnUuid& localSystemId,
    const QUrl& url,
    bool storePassword);

} // namespace helpers
