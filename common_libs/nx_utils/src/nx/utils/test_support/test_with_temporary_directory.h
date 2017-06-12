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

private:
    QString m_tmpDir;
};

} // namespace test
} // namespace utils
} // namespace nx
