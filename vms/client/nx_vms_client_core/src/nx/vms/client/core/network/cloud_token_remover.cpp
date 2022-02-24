// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_token_remover.h"

#include <nx/utils/log/log.h>
#include <nx/vms/client/core/ini.h>

using namespace nx::cloud::db::api;

namespace nx::vms::client::core {

CloudTokenRemover::~CloudTokenRemover()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_connectionMap.clear();
}

void CloudTokenRemover::removeToken(const std::string& token, ConnectionPtr connection)
{
    if (token.empty() || !connection)
        return;

    auto handler =
        [this, token](nx::cloud::db::api::ResultCode resultCode)
        {
            NX_INFO(this, "Token removed with result: %1", resultCode);
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_connectionMap.erase(token);
        };

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_connectionMap.emplace(token, connection);
    connection->oauthManager()->deleteToken(token, std::move(handler));
}

} // namespace nx::vms::client::core
