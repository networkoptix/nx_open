#pragma once

#include <QtCore/QString>

#include <nx/sql/types.h>

#include "config.h"

class QnSettings;

namespace nx::analytics::db {

class Settings
{
public:
    QString path;
    nx::sql::ConnectionOptions dbConnectionOptions;
    std::chrono::milliseconds maxCachedObjectLifeTime = kMaxCachedObjectLifeTime;

    void load(const QnSettings& settings);
};

} // namespace nx::analytics::db
