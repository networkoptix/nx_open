#pragma once

#include <utils/common/credentials.h>

#include <nx/utils/log/log_level.h>
#include <nx/utils/uuid.h>
#include <nx/utils/url.h>

namespace nx::vms::client::core {
namespace helpers {

extern const nx::utils::log::Tag kCredentialsLogTag;

void clearSavedPasswords();
void storeConnection(const QnUuid& localSystemId, const QString& systemName, const nx::utils::Url &url);
void removeConnection(const QnUuid& localSystemId, const nx::utils::Url& url = nx::utils::Url());
void storeCredentials(const QnUuid& localSystemId, const nx::vms::common::Credentials& credentials);
void removeCredentials(const QnUuid& localSystemId, const QString& userName = QString());
common::Credentials getCredentials(const QnUuid& localSystemId, const QString& userName);
bool hasCredentials(const QnUuid& localSystemId);

void saveCloudCredentials(const common::Credentials& credentials);
void forgetSavedCloudCredentials(bool keepUser = false);

QnUuid preferredCloudServer(const QString& systemId);
void savePreferredCloudServer(const QString& systemId, const QnUuid& serverId);

QString serverCloudHost(const QString& systemId, const QnUuid& serverId);

} // namespace helpers
} // namespace nx::vms::client::core
