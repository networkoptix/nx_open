// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/log/log_level.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/credentials_manager.h>

namespace nx::vms::client::core {

struct CloudAuthData;

namespace helpers {

NX_VMS_CLIENT_CORE_API extern const nx::utils::log::Tag kCredentialsLogTag;
NX_VMS_CLIENT_CORE_API extern const char* const kCloudUrlSchemeName;

NX_VMS_CLIENT_CORE_API void storeConnection(
    const QnUuid& localSystemId,
    const QString& systemName,
    const nx::utils::Url& url);

NX_VMS_CLIENT_CORE_API void removeConnection(
    const QnUuid& localSystemId,
    const nx::utils::Url& url = nx::utils::Url());

NX_VMS_CLIENT_CORE_API void removePasswords(
    const QnUuid& localSystemId,
    const QString& userName = QString());

/** Try to load password cloud credentials from older versions, 4.2 and below.*/
NX_VMS_CLIENT_CORE_API nx::network::http::Credentials loadCloudPasswordCredentials();

NX_VMS_CLIENT_CORE_API CloudAuthData loadCloudCredentials();
NX_VMS_CLIENT_CORE_API void saveCloudCredentials(const CloudAuthData& credentials);

NX_VMS_CLIENT_CORE_API std::optional<QnUuid> preferredCloudServer(const QString& systemId);
NX_VMS_CLIENT_CORE_API void savePreferredCloudServer(
    const QString& systemId, const QnUuid& serverId);

/** Server address to access it using cloud sockets. */
NX_VMS_CLIENT_CORE_API QString serverCloudHost(const QString& systemId, const QnUuid& serverId);
NX_VMS_CLIENT_CORE_API bool isCloudUrl(const nx::utils::Url& url);

} // namespace helpers
} // namespace nx::vms::client::core
