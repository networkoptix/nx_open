#pragma once

#include <QtCore/QString>
#include <QtCore/QDir>

namespace nx::utils::test {

class NX_UTILS_API TestWithTemporaryDirectory
{
public:
    /**
     * Temporary directory path can be passed explitly. In this case folder contents will be
     * automatically cleared. Aternatively, global temp path variable will be used. If it is empty,
     * home folder will be used. In these cases module name will be used as a distinction subfolder.
     */
    TestWithTemporaryDirectory(const QString& moduleName, const QString& tmpDir = QString());
    virtual ~TestWithTemporaryDirectory();

    QString testDataDir() const;

private:
    QDir m_tmpDir;
};

} // namespace nx::utils::test
