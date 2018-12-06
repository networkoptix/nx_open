#pragma once

#include <QtCore/QUrl>

#include <nx/vms/client/core/common/utils/encoded_credentials.h>

#include <nx/utils/log/log_level.h>
#include <nx/utils/uuid.h>
#include <nx/utils/url.h>

namespace nx::vms::client::core {
namespace helpers {

extern const nx::utils::log::Tag kCredentialsLogTag;

void clearSavedPasswords();
void storeConnection(const QnUuid& localSystemId, const QString& systemName, const nx::utils::Url &url);
void removeConnection(const QnUuid& localSystemId, const nx::utils::Url& url = nx::utils::Url());
void storeCredentials(const QnUuid& localSystemId, const EncodedCredentials& credentials);
void removeCredentials(const QnUuid& localSystemId, const QString& userName = QString());
EncodedCredentials getCredentials(const QnUuid& localSystemId, const QString& userName);
bool hasCredentials(const QnUuid& localSystemId);

} // namespace helpers
} // namespace nx::vms::client::core
