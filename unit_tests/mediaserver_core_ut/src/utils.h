#ifndef __MEDIASERVER_CORE_UT_UTIL_H__
#define __MEDIASERVER_CORE_UT_UTIL_H__

#include <boost/optional.hpp>
#include <QtCore>

namespace nx
{
namespace ut
{
namespace cfg
{

struct Config
{
    // all tests tmp work dir
    QString tmpDir;

    // storage tests ftp and smb storages urls
    QString ftpUrl;
    QString smbUrl;
};

} // namespace cfg
namespace utils
{

// if successfull, returns directory name
boost::optional<QString> createRandomDir();

// auto creation and cleaning up of working directory helper
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

#endif
