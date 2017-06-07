#pragma once

#include <boost/optional.hpp>

#include <QtCore>

namespace nx {
namespace ut {
namespace cfg {

struct Config
{
    // Every test tmp work dir.
    QString tmpDir;

    // Storage tests ftp and smb storages urls.
    QString ftpUrl;
    QString smbUrl;
};

nx::ut::cfg::Config& configInstance();

} // namespace cfg

namespace utils {

// If successfull, returns directory name.
boost::optional<QString> createRandomDir();

// Auto creation and cleaning up of working directory helper.
class WorkDirResource
{
public:
    WorkDirResource(const QString& path = QString());
    ~WorkDirResource();

    boost::optional<QString> getDirName() const;

private:
    boost::optional<QString> m_workDir;
};

bool validateAndOrCreatePath(const QString &path);

} // namespace utils
} // namespace ut
} // namespace nx
