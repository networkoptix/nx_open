// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QJsonValue>

#include "user_access_data.h"

namespace nx::network::rest::audit {

struct Record: UserSession
{
    std::optional<QJsonValue> apiInfo;

    Record(Record&&) = default;
    Record(const Record&) = default;
    Record& operator=(Record&&) = default;
    Record& operator=(const Record&) = default;
    bool operator==(const Record&) const = default;

    Record(UserSession userSession, std::optional<QJsonValue> apiInfo = {}):
        UserSession(std::move(userSession)), apiInfo(std::move(apiInfo))
    {
    }
};

class Manager
{
public:
    virtual ~Manager() = default;
    virtual void addAuditRecord(const Record& record) = 0;
};

} // namespace nx::network::rest::audit
