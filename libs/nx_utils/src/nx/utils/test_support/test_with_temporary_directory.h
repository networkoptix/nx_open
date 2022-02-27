// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QDir>

namespace nx::utils::test {

class NX_UTILS_API TestWithTemporaryDirectory
{
public:
    /**
     * Temporary directory path can be passed explicitly. In this case, folder contents will be
     * automatically cleared. Aternatively, global temporary path variable
     * (TestOptions::temporaryDirectoryPath) will be used.
     * If it is empty, home folder will be used. In these cases TestOptions::moduleName()
     * is used as a distinction subfolder.
     */
    TestWithTemporaryDirectory(const QString& tmpDir = QString());
    virtual ~TestWithTemporaryDirectory();

    QString testDataDir() const;

private:
    QDir m_tmpDir;
};

} // namespace nx::utils::test
