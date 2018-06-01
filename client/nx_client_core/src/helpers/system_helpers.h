#pragma once

#include <QtCore/QUrl>

#include <nx/utils/log/log_level.h>
#include <nx/utils/uuid.h>
#include <nx/utils/url.h>
#include <utils/common/encoded_credentials.h>

namespace nx {
namespace client {
namespace core {
namespace helpers {

extern const nx::utils::log::Tag kCredentialsLogTag;

void clearSavedPasswords();
void storeConnection(const QnUuid& localSystemId, const QString& systemName, const nx::utils::Url &url);
void removeConnection(const QnUuid& localSystemId, const nx::utils::Url& url = nx::utils::Url());
void storeCredentials(const QnUuid& localSystemId, const QnEncodedCredentials& credentials);
void removeCredentials(const QnUuid& localSystemId, const QString& userName = QString());
QnEncodedCredentials getCredentials(const QnUuid& localSystemId, const QString& userName);
bool hasCredentials(const QnUuid& localSystemId);

} // namespace helpers
} // namespace core
} // namespace client
} // namespace nx
