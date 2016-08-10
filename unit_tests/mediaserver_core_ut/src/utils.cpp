// Needed for boost 1.60.0 seed_rng.hpp
#undef NULL
#define NULL (0)

#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include "utils.h"
#include <cassert>
#include <stdio.h>

extern nx::ut::cfg::Config config;

namespace nx
{
namespace ut
{
namespace utils
{

boost::optional<QString> createRandomDir()
{
    assert(!config.tmpDir.isEmpty());
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    QString dirName = QString::fromStdString(boost::uuids::to_string(uuid));;
    QString fullPath(QDir(config.tmpDir).absoluteFilePath(dirName));
    assert(!QDir().exists(fullPath));
    if (QDir().exists(fullPath))
        return boost::none;
    if (!QDir().mkpath(fullPath))
        return boost::none;
    return fullPath;
}

WorkDirResource::WorkDirResource(const QString& path)
    : m_workDir(path.isEmpty() ? createRandomDir() : path) {}
WorkDirResource::~WorkDirResource()
{
    if (m_workDir)
    {
        if (!QDir(*m_workDir).removeRecursively())
            fprintf(stderr, "Recursive cleaning up %s failed\n", (*m_workDir).toLatin1().constData());
    }
}

boost::optional<QString> WorkDirResource::getDirName() const
{
    return m_workDir;
}

bool validateAndOrCreatePath(const QString &path)
{
    if (path.isNull() || path.isEmpty())
        return false;
    QDir dir(path);
    if (dir.exists() || dir.mkpath(path))
        return true;
    return false;
}

} // namespace utils
} // namespace ut
} // namespace nx


