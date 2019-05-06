#pragma once

#include <QtCore/QString>

#include <nx/sql/types.h>

class QnSettings;

namespace nx {
namespace analytics {
namespace storage {

class Settings
{
public:
    QString path;
    nx::sql::ConnectionOptions dbConnectionOptions;

    void load(const QnSettings& settings);
};

} // namespace storage
} // namespace analytics
} // namespace nx
