#pragma once

#include <boost/optional.hpp>

#include <QtCore>

namespace nx {
namespace ut {
namespace cfg {

struct Config
{
    /** Temporary working directory for all tests. */
    QString tmpDir;
    /** Ftp storage url. */
    QString ftpUrl;
    /** Samba storage url. */
    QString smbUrl;
};

nx::ut::cfg::Config& configInstance();

} // namespace cfg

namespace utils {

/**
 * @return Directory name on success, boost::none on failure.
 */
boost::optional<QString> createRandomDir();

/**
 * Creates the specified directory in the constructor and removes it in the destructor.
 */
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
