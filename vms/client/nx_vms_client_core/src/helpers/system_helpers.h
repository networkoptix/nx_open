// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/log/log_level.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/credentials_manager.h>

namespace nx::vms::client::core {

struct CloudAuthData;

namespace helpers {

extern const nx::utils::log::Tag kCredentialsLogTag;
extern const char* const kCloudUrlSchemeName;

void storeConnection(const QnUuid& localSystemId, const QString& systemName, const nx::utils::Url &url);
void removeConnection(const QnUuid& localSystemId, const nx::utils::Url& url = nx::utils::Url());

void removePasswords(const QnUuid& localSystemId, const QString& userName = QString());

/** Try to load password cloud credentials from older versions, 4.2 and below.*/
nx::network::http::Credentials loadCloudPasswordCredentials();
void forgetSavedCloudPassword();

CloudAuthData loadCloudCredentials();
void saveCloudCredentials(const CloudAuthData& credentials);

QnUuid preferredCloudServer(const QString& systemId);
void savePreferredCloudServer(const QString& systemId, const QnUuid& serverId);

QString serverCloudHost(const QString& systemId, const QnUuid& serverId);
bool isCloudUrl(const nx::utils::Url& url);

} // namespace helpers
} // namespace nx::vms::client::core
