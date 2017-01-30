#pragma once

#include <QtCore/QUrl>

#include <nx/utils/uuid.h>
#include <utils/common/credentials.h>

namespace nx {
namespace client {
namespace core {
namespace helpers {

void storeConnection(const QnUuid& localSystemId, const QString& systemName, const QUrl& url);
void removeConnection(const QnUuid& localSystemId, const QUrl& url = QUrl());
void storeCredentials(const QnUuid& localSystemId, const QnCredentials& credentials);
void removeCredentials(const QnUuid& localSystemId, const QString& userName = QString());
QnCredentials getCredentials(const QnUuid& localSystemId, const QString& userName);
bool hasCredentials(const QnUuid& localSystemId);

} // namespace helpers
} // namespace core
} // namespace client
} // namespace nx
