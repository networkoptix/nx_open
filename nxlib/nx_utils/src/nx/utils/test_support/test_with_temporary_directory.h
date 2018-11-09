#pragma once

#include <QtCore/QString>
#include <QtCore/QDir>

namespace nx {
namespace utils {
namespace test {

class NX_UTILS_API TestWithTemporaryDirectory
{
public:
    TestWithTemporaryDirectory(QString moduleName, QString tmpDir);
    ~TestWithTemporaryDirectory();

    QString testDataDir() const;

private:
    QDir m_tmpDir;
};

} // namespace test
} // namespace utils
} // namespace nx
