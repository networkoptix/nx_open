#pragma once

#include <QtCore/QString>

namespace nx {
namespace utils {
namespace test {

class NX_UTILS_API TestWithTemporaryDirectory
{
public:
    TestWithTemporaryDirectory(QString moduleName, QString tmpDir);
    ~TestWithTemporaryDirectory();

    QString testDataDir() const;

    static void setTemporaryDirectoryPath(const QString& path);
    static QString temporaryDirectoryPath();

private:
    QString m_tmpDir;

    static QString sTemporaryDirectoryPath;
};

} // namespace test
} // namespace utils
} // namespace nx
