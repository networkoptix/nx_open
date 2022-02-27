// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/cloud/db/api/connection.h>
#include <nx/utils/thread/mutex.h>

namespace nx::vms::client::core {

class CloudTokenRemover
{
public:
    using ConnectionPtr = std::shared_ptr<nx::cloud::db::api::Connection>;

public:
    ~CloudTokenRemover();

    void removeToken(const std::string& token, ConnectionPtr connection);

private:
    nx::Mutex m_mutex;
    std::map<std::string, ConnectionPtr> m_connectionMap;
};

} // namespace nx::vms::client::core
