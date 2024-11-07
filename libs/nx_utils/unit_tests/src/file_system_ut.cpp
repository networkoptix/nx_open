// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QFile>

#include <nx/utils/file_system.h>
#include <nx/utils/test_support/test_options.h>

static const QString kTestDirectoryName("__tmp_fs_test"); //< TODO: Remove it.
static const QString kSourceDirectoryName("src");
static const QString kTargetDirectoryName("dst");

namespace nx {
namespace utils {
namespace test {

class CopyMoveTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        QDir tempDir(nx::TestOptions::temporaryDirectoryPath());
        QDir rootDir(tempDir.absoluteFilePath(kTestDirectoryName));

        rootDir.removeRecursively();
        ASSERT_TRUE(QDir().mkpath(rootDir.absolutePath()));

        ASSERT_TRUE(rootDir.mkdir(kSourceDirectoryName));
        sourceDir.setPath(rootDir.absoluteFilePath(kSourceDirectoryName));
        ASSERT_TRUE(rootDir.mkdir(kTargetDirectoryName));
        targetDir.setPath(rootDir.absoluteFilePath(kTargetDirectoryName));
    }

    virtual void TearDown() override
    {
        QDir tempDir(nx::TestOptions::temporaryDirectoryPath());
        QDir rootDir(tempDir.absoluteFilePath(kTestDirectoryName));
        ASSERT_TRUE(rootDir.removeRecursively());
    }

    enum class Location
    {
        source,
        target
    };

    QString createPath(const QString& path, Location location = Location::source)
    {
        const auto dir = (location == Location::source) ? sourceDir : targetDir;
        if (dir.mkpath(path))
            return dir.absoluteFilePath(path);
        return QString();
    }

    QString createFile(const QString& fileName, Location location = Location::source,
        const QByteArray& data = QByteArray())
    {
        const auto dir = (location == Location::source) ? sourceDir : targetDir;
        QFile f(dir.absoluteFilePath(fileName));
        if (f.open(QFile::WriteOnly))
        {
            f.write(data);
            f.close();
            return f.fileName();
        }
        return QString();
    }

    QString createSymLink(
        const QString& linkName,
        const QString& linkTarget,
        Location location = Location::source)
    {
        const auto dir = (location == Location::source) ? sourceDir : targetDir;
        const auto linkFileName = dir.absoluteFilePath(linkName);
        if (QFile::link(linkTarget, linkFileName))
            return linkFileName;
        return QString();
    }

    bool filesAreEqual(const QString& fileName1, const QString& fileName2)
    {
        QFile file1(fileName1);
        if (!file1.open(QFile::ReadOnly))
            return false;
        const auto data1 = file1.readAll();
        file1.close();

        QFile file2(fileName2);
        if (!file2.open(QFile::ReadOnly))
            return false;
        const auto data2 = file2.readAll();
        file2.close();

        return data1 == data2;
    }

    int entriesCount(const QDir& dir, bool recursive = false)
    {
        const auto flags = QDir::Files | QDir::Dirs
            | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System;

        int count = 0;

        for (const auto& entryInfo: dir.entryInfoList(flags))
        {
            ++count;
            if (entryInfo.isDir() && recursive)
                count += entriesCount(entryInfo.absoluteFilePath(), recursive);
        }

        return count;
    }

    QDir sourceDir;
    QDir targetDir;
};

TEST_F(CopyMoveTest, ensureDirectory)
{
    const QDir dir(sourceDir.absoluteFilePath("dir/subdir"));
    ASSERT_TRUE(file_system::ensureDir(dir));

    ASSERT_TRUE(dir.exists());
    ASSERT_TRUE(QFileInfo(dir.absolutePath()).isDir());
}

TEST_F(CopyMoveTest, copyFile)
{
    const QString fileName("file");

    const auto srcName = createFile(fileName);
    ASSERT_FALSE(srcName.isEmpty());

    auto result = file_system::copy(srcName, targetDir.absoluteFilePath(fileName));
    ASSERT_EQ(result.code, file_system::Result::ok);
    ASSERT_TRUE(QFileInfo(targetDir.filePath(fileName)).exists());
}

TEST_F(CopyMoveTest, copyFileToDirectory)
{
    const QString fileName("file");

    const auto srcName = createFile(fileName);
    ASSERT_FALSE(srcName.isEmpty());

    auto result = file_system::copy(srcName, targetDir.absolutePath());
    ASSERT_EQ(result.code, file_system::Result::ok);
    ASSERT_TRUE(QFileInfo(targetDir.filePath(fileName)).exists());
}

TEST_F(CopyMoveTest, copyFileToNotExistingDirectory)
{
    const QString fileName("file");

    const auto srcName = createFile(fileName);
    ASSERT_FALSE(srcName.isEmpty());

    const auto dstName = targetDir.absoluteFilePath("not/existing/directory/" + fileName);

    auto result = file_system::copy(srcName, targetDir.absoluteFilePath(dstName));
    ASSERT_EQ(result.code, file_system::Result::cannotCreateDirectory);

    result = file_system::copy(
        srcName, targetDir.absoluteFilePath(dstName), file_system::CreateTargetPath);
    ASSERT_EQ(result.code, file_system::Result::ok);

    ASSERT_TRUE(QFileInfo(dstName).exists());
}

TEST_F(CopyMoveTest, copyFileOverwrite)
{
    const QString fileName("file");

    const auto srcName = createFile(fileName, Location::source, "src");
    ASSERT_FALSE(srcName.isEmpty());

    const auto dstName = createFile(fileName, Location::target, "tgt");
    ASSERT_FALSE(dstName.isEmpty());

    auto result = file_system::copy(srcName, dstName);
    ASSERT_EQ(result.code, file_system::Result::alreadyExists);

    result = file_system::copy(
        srcName, targetDir.absoluteFilePath(dstName), file_system::SkipExisting);
    ASSERT_EQ(result.code, file_system::Result::ok);
    ASSERT_FALSE(filesAreEqual(srcName, dstName));

    result = file_system::copy(
        srcName, targetDir.absoluteFilePath(dstName), file_system::OverwriteExisting);
    ASSERT_EQ(result.code, file_system::Result::ok);
    ASSERT_TRUE(filesAreEqual(srcName, dstName));
}

TEST_F(CopyMoveTest, copyEmptyDirectory)
{
    const QString dirName("dir");

    const auto srcName = createPath(dirName);
    ASSERT_FALSE(srcName.isEmpty());

    auto result = file_system::copy(srcName, targetDir.absolutePath());
    ASSERT_EQ(result.code, file_system::Result::ok);

    QFileInfo targetInfo(targetDir.filePath(dirName));
    ASSERT_TRUE(targetInfo.exists());
    ASSERT_TRUE(targetInfo.isDir());
}

TEST_F(CopyMoveTest, copyDirectory)
{
    const QString dirName("dir");

    const auto srcName = createPath(dirName);
    ASSERT_FALSE(srcName.isEmpty());

    createFile("dir/file1");
    createFile("dir/file2");
    createPath("dir/subdir/subsubdir");
    createFile("dir/subdir/subsubdir/inner_file");

    auto result = file_system::copy(srcName, targetDir.absolutePath());
    ASSERT_EQ(result.code, file_system::Result::ok);

    ASSERT_EQ(
        entriesCount(srcName),
        entriesCount(targetDir.absoluteFilePath(dirName)));
    ASSERT_EQ(
        entriesCount(srcName, true),
        entriesCount(targetDir.absoluteFilePath(dirName), true));
}

TEST_F(CopyMoveTest, copyDirectoryOverwrite)
{
    const QString dirName("dir");

    const auto srcName = createPath(dirName);
    ASSERT_FALSE(srcName.isEmpty());

    createFile("dir/file1");
    createFile("dir/file2");
    createPath("dir/subdir/subsubdir");
    createFile("dir/subdir/subsubdir/inner_file", Location::source, "src");

    createPath("dir/subdir/subsubdir", Location::target);
    createFile("dir/subdir/subsubdir/inner_file", Location::target, "dst");

    auto result = file_system::copy(srcName, targetDir.absolutePath());
    ASSERT_EQ(result.code, file_system::Result::alreadyExists);

    auto skipResult = file_system::copy(srcName, targetDir.absolutePath(),
        file_system::SkipExisting);
    ASSERT_EQ(skipResult.code, file_system::Result::ok);
    ASSERT_EQ(
        entriesCount(srcName, true),
        entriesCount(targetDir.absoluteFilePath(dirName), true));
    ASSERT_FALSE(filesAreEqual(
        sourceDir.absoluteFilePath("dir/subdir/subsubdir/inner_file"),
        targetDir.absoluteFilePath("dir/subdir/subsubdir/inner_file")));

    auto overwriteResult = file_system::copy(srcName, targetDir.absolutePath(),
        file_system::OverwriteExisting);
    ASSERT_EQ(overwriteResult.code, file_system::Result::ok);
    ASSERT_EQ(
        entriesCount(srcName, true),
        entriesCount(targetDir.absoluteFilePath(dirName), true));
    ASSERT_TRUE(filesAreEqual(
        sourceDir.absoluteFilePath("dir/subdir/subsubdir/inner_file"),
        targetDir.absoluteFilePath("dir/subdir/subsubdir/inner_file")));
}

TEST_F(CopyMoveTest, Move_SourceIsFile_TargetExists_TargetIsFile)
{
    const QString srcDirName("src");
    createPath(srcDirName);

    const QString tgtDirName("tgt");
    createPath(tgtDirName, Location::target);

    auto srcName = createFile("src/file1", Location::source, "src");
    auto tgtName = createFile("tgt/file1", Location::target);

    ASSERT_EQ(file_system::Result::alreadyExists, file_system::move(srcName, tgtName).code);
    ASSERT_EQ(
        file_system::Result::ok,
        file_system::move(srcName, tgtName, /*replace*/ true).code);

    QFile file(tgtName);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    ASSERT_EQ(QByteArray("src"), file.readAll());
    ASSERT_FALSE(QFile(srcName).exists());
}

TEST_F(CopyMoveTest, Move_SourceIsFile_TargetExists_TargetIsFolder_FileInTargetExists)
{
    const QString srcDirName("src");
    createPath(srcDirName);

    const QString tgtDirName("tgt");
    const auto tgtDirPath = createPath(tgtDirName, Location::target);

    auto srcName = createFile("src/file1", Location::source, "src");
    auto tgtName = createFile("tgt/file1", Location::target);

    ASSERT_EQ(file_system::Result::alreadyExists, file_system::move(srcName, tgtDirPath).code);
    ASSERT_EQ(
        file_system::Result::ok,
        file_system::move(srcName, tgtDirPath, /*replace*/ true).code);

    QFile file(tgtName);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    ASSERT_EQ(QByteArray("src"), file.readAll());
    ASSERT_FALSE(QFile(srcName).exists());
}

TEST_F(CopyMoveTest, Move_SourceIsFile_TargetExists_TargetIsFolder_FileInTargetDoesNotExist)
{
    const QString srcDirName("src");
    createPath(srcDirName);

    const QString tgtDirName("tgt");
    const auto tgtDirPath = createPath(tgtDirName, Location::target);

    auto srcName = createFile("src/file1", Location::source, "src");
    auto tgtName = createFile("tgt/file1", Location::target);
    ASSERT_TRUE(QFile(tgtName).remove());

    ASSERT_EQ(file_system::Result::ok, file_system::move(srcName, tgtDirPath).code);

    QFile file(tgtName);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    ASSERT_EQ(QByteArray("src"), file.readAll());
    ASSERT_FALSE(QFile(srcName).exists());
}

TEST_F(CopyMoveTest, Move_SourceIsFile_TargetDoesNotExist_EnclosingExists)
{
    const QString srcDirName("src");
    createPath(srcDirName);

    const QString tgtDirName("tgt");
    const auto tgtDirPath = createPath(tgtDirName, Location::target);

    auto srcName = createFile("src/file1", Location::source, "src");
    auto tgtName = createFile("tgt/file2", Location::target);
    ASSERT_TRUE(QFile(tgtName).remove());

    ASSERT_EQ(file_system::Result::ok, file_system::move(srcName, tgtName).code);

    QFile file(tgtName);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    ASSERT_EQ(QByteArray("src"), file.readAll());
    ASSERT_FALSE(QFile(srcName).exists());
}

TEST_F(CopyMoveTest, Move_SourceIsFile_TargetDoesNotExist_EnclosingDoesNotExist)
{
    const QString srcDirName("src");
    createPath(srcDirName);

    const QString tgtDirName("tgt");
    const auto tgtDirPath = createPath(tgtDirName, Location::target);
    ASSERT_TRUE(QDir(tgtDirPath).removeRecursively());

    auto srcName = createFile("src/file1", Location::source, "src");

    ASSERT_EQ(
        file_system::Result::targetFolderDoesNotExist,
        file_system::move(srcName, tgtDirPath + "/file2").code);
}

TEST_F(CopyMoveTest, Move_SourceIsFolder_TargetExists_TargetIsFile)
{
    const auto srcDirPath = createPath("src");
    const auto tgtDirPath = createPath("tgt", Location::target);

    auto srcFilePath = createFile("src/file1", Location::source, "src");
    auto tgtFilePath = createFile("tgt/file2", Location::target);

    ASSERT_EQ(
        file_system::Result::cannotMove,
        file_system::move(srcDirPath, tgtFilePath).code);
}

TEST_F(CopyMoveTest, Move_SourceIsFolder_TargetExists_TargetIsFolder)
{
    const auto srcDirPath = createPath("src");
    const auto tgtDirPath = createPath("tgt", Location::target);

    auto srcFilePath = createFile("src/file1", Location::source, "src");

    ASSERT_EQ(
        file_system::Result::ok,
        file_system::move(srcDirPath, tgtDirPath).code);

    QFile file(tgtDirPath + "/src/file1");
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    ASSERT_EQ(QByteArray("src"), file.readAll());
    ASSERT_FALSE(QFile(srcFilePath).exists());
}

TEST_F(CopyMoveTest, Move_SourceIsFolder_TargetDoesNotExist_EnclosingExists)
{
    const auto srcDirPath = createPath("src");
    const auto tgtDirPath = createPath("tgt", Location::target);

    auto srcFilePath = createFile("src/file1", Location::source, "src");

    ASSERT_EQ(
        file_system::Result::ok,
        file_system::move(srcDirPath, tgtDirPath + "/not_existing").code);

    QFile file(tgtDirPath + "/not_existing/file1");
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    ASSERT_EQ(QByteArray("src"), file.readAll());
    ASSERT_FALSE(QFile(srcFilePath).exists());
}

TEST_F(CopyMoveTest, Move_SourceIsFolder_TargetDoesNotExist_EnclosingDoesNotExist)
{
    const auto srcDirPath = createPath("src");
    const auto tgtDirPath = createPath("tgt", Location::target);
    ASSERT_TRUE(QDir(tgtDirPath).removeRecursively());

    auto srcFilePath = createFile("src/file1", Location::source, "src");

    ASSERT_EQ(
        file_system::Result::targetFolderDoesNotExist,
        file_system::move(srcDirPath, tgtDirPath + "/not_existing").code);
}

TEST_F(CopyMoveTest, MoveFolderContents_ReplaceExistingTrue)
{
    const auto srcDirPath = createPath("src");
    createPath("src/dir1");
    createPath("src/dir2");
    createFile("src/dir1/file1", Location::source, "src");
    createFile("src/dir2/file2", Location::source, "src");
    createFile("src/file0", Location::source, "src");

    const auto tgtDirPath = createPath("tgt", Location::target);
    createFile("tgt/file0", Location::target);
    createPath("tgt/dir1", Location::target);
    createFile("tgt/dir1/file1", Location::target);

    ASSERT_EQ(
        file_system::Result::ok,
        file_system::moveFolderContents(srcDirPath, tgtDirPath, /*replace*/ true).code);

    ASSERT_FALSE(QDir(srcDirPath + "/dir1").exists());
    ASSERT_FALSE(QDir(srcDirPath + "/dir2").exists());
    ASSERT_FALSE(QFile(srcDirPath + "/dir1/file1").exists());
    ASSERT_FALSE(QFile(srcDirPath + "/dir2/file2").exists());
    ASSERT_FALSE(QFile(srcDirPath + "/file0").exists());

    ASSERT_TRUE(QDir(tgtDirPath + "/dir1").exists());
    ASSERT_TRUE(QDir(tgtDirPath + "/dir2").exists());

    auto file1 = QFile(tgtDirPath + "/dir1/file1");
    ASSERT_TRUE(file1.open(QIODevice::ReadOnly));
    ASSERT_EQ(QByteArray("src"), file1.readAll());

    auto file2 = QFile(tgtDirPath + "/dir2/file2");
    ASSERT_TRUE(file2.open(QIODevice::ReadOnly));
    ASSERT_EQ(QByteArray("src"), file2.readAll());

    auto file3 = QFile(tgtDirPath + "/file0");
    ASSERT_TRUE(file3.open(QIODevice::ReadOnly));
    ASSERT_EQ(QByteArray("src"), file3.readAll());
}

TEST_F(CopyMoveTest, MoveFolderContents_ReplaceExistingFalse)
{
    const auto srcDirPath = createPath("src");
    createPath("src/dir1");
    createPath("src/dir2");
    createFile("src/dir1/file1", Location::source, "src");
    createFile("src/dir2/file2", Location::source, "src");
    createFile("src/file0", Location::source, "src");

    const auto tgtDirPath = createPath("tgt", Location::target);
    createFile("tgt/file0", Location::target);
    createPath("tgt/dir1", Location::target);
    createFile("tgt/dir1/file1", Location::target);

    ASSERT_EQ(
        file_system::Result::ok,
        file_system::moveFolderContents(srcDirPath, tgtDirPath, /*replace*/ false).code);

    ASSERT_TRUE(QDir(srcDirPath + "/dir1").exists());
    ASSERT_FALSE(QDir(srcDirPath + "/dir2").exists());
    ASSERT_TRUE(QFile(srcDirPath + "/dir1/file1").exists());
    ASSERT_FALSE(QFile(srcDirPath + "/dir2/file2").exists());
    ASSERT_TRUE(QFile(srcDirPath + "/file0").exists());

    ASSERT_TRUE(QDir(tgtDirPath + "/dir1").exists());
    ASSERT_TRUE(QDir(tgtDirPath + "/dir2").exists());

    auto file1 = QFile(tgtDirPath + "/dir1/file1");
    ASSERT_TRUE(file1.open(QIODevice::ReadOnly));
    ASSERT_NE(QByteArray("src"), file1.readAll());

    auto file2 = QFile(tgtDirPath + "/dir2/file2");
    ASSERT_TRUE(file2.open(QIODevice::ReadOnly));
    ASSERT_EQ(QByteArray("src"), file2.readAll());

    auto file3 = QFile(tgtDirPath + "/file0");
    ASSERT_TRUE(file3.open(QIODevice::ReadOnly));
    ASSERT_NE(QByteArray("src"), file3.readAll());
}

#if defined(Q_OS_UNIX)
    TEST_F(CopyMoveTest, readSymLinkTarget)
    {
        const QString linkName("link");

        createFile("file");

        const auto srcName = createSymLink(linkName, "file");
        ASSERT_FALSE(srcName.isEmpty());

        QString symLinkTarget = file_system::symLinkTarget(srcName);
        ASSERT_EQ(symLinkTarget, "file");
    }

    TEST_F(CopyMoveTest, copyFileLink)
    {
        const QString linkName("link");

        createFile("file");

        const auto srcName = createSymLink(linkName, "file");
        ASSERT_FALSE(srcName.isEmpty());

        auto result = file_system::copy(srcName, targetDir.absolutePath());
        ASSERT_EQ(result.code, file_system::Result::ok);
        const QFileInfo linkInfo(targetDir.filePath(linkName));

        ASSERT_FALSE(linkInfo.exists());

        result = file_system::copy(sourceDir.absoluteFilePath("file"), targetDir.absolutePath());
        ASSERT_EQ(result.code, file_system::Result::ok);

        ASSERT_TRUE(linkInfo.isSymLink());
    }

    TEST_F(CopyMoveTest, copyDirectoryLink)
    {
        const auto srcName = createPath("dir");
        createFile("dir/file");
        createSymLink("link", "dir");

        auto result = file_system::copy(sourceDir.absoluteFilePath("link"), targetDir.absolutePath());
        ASSERT_EQ(result.code, file_system::Result::ok);

        const QFileInfo linkInfo(targetDir.filePath("link"));
        ASSERT_FALSE(linkInfo.exists());

        result = file_system::copy(sourceDir.absoluteFilePath("dir"), targetDir.absolutePath());
        ASSERT_EQ(result.code, file_system::Result::ok);

        ASSERT_TRUE(linkInfo.isSymLink());
        ASSERT_EQ(
            entriesCount(srcName, true),
            entriesCount(targetDir.absoluteFilePath("dir"), true));
    }

    TEST_F(CopyMoveTest, followSymLinks)
    {
        createPath("dir");
        createFile("dir/file");
        createSymLink("link", "dir");
        createSymLink("dir/link", "file");

        auto result = file_system::copy(sourceDir.absoluteFilePath("link"), targetDir.absolutePath(),
            file_system::FollowSymLinks);
        ASSERT_EQ(result.code, file_system::Result::ok);

        const QFileInfo dirInfo(targetDir.filePath("link"));
        ASSERT_TRUE(dirInfo.isDir());
        ASSERT_FALSE(dirInfo.isSymLink());

        const QFileInfo fileInfo(targetDir.filePath("link/link"));
        ASSERT_TRUE(fileInfo.isFile());
        ASSERT_FALSE(dirInfo.isSymLink());
    }

    TEST_F(CopyMoveTest, symLinkOverlap)
    {
        const auto srcFile = createFile("file");
        ASSERT_FALSE(srcFile.isEmpty());
        const auto srcLink = createSymLink("link", "file");
        ASSERT_FALSE(srcLink.isEmpty());

        auto result = file_system::copy(srcLink, srcFile);
        ASSERT_EQ(result.code, file_system::Result::sourceAndTargetAreSame);
    }

    TEST_F(CopyMoveTest, directorySelfOverlap)
    {
        createPath("dir/dir/dir");
        createSymLink("dir/dir/dir/dir", "../dir");

        auto result = file_system::copy(
            sourceDir.absoluteFilePath("dir/dir"), sourceDir.absolutePath());
        ASSERT_EQ(result.code, file_system::Result::sourceAndTargetAreSame);
    }

#endif //defined(Q_OS_UNIX)

TEST_F(CopyMoveTest, directoryOverlap)
{
    createPath("dir/dir/dir");
    createFile("dir/dir/dir/file");

    const auto initialEntriesCount = entriesCount(sourceDir, true);

    auto result = file_system::copy(
        sourceDir.absoluteFilePath("dir/dir"), sourceDir.absolutePath());
    ASSERT_EQ(result.code, file_system::Result::ok);

    ASSERT_TRUE(QFile(sourceDir.absoluteFilePath("dir/dir/file")).exists());

    const auto newEntriesCount = entriesCount(sourceDir, true);
    ASSERT_EQ(initialEntriesCount + 1, newEntriesCount);
}


TEST_F(CopyMoveTest, simpleOverlap)
{
    const auto srcFile = createFile("file");
    ASSERT_FALSE(srcFile.isEmpty());

    auto result = file_system::copy(srcFile, srcFile);
    ASSERT_EQ(result.code, file_system::Result::sourceAndTargetAreSame);

    const auto srcDir = createFile("dir");
    ASSERT_FALSE(srcDir.isEmpty());

    result = file_system::copy(srcDir, srcDir);
    ASSERT_EQ(result.code, file_system::Result::sourceAndTargetAreSame);
}

static void isRelativePathSafeTest(bool expectedResult, const char* path)
{
    EXPECT_EQ(expectedResult, file_system::isRelativePathSafe(path)) << path;
}

TEST(FileSystem, isRelativePathSafe)
{
    isRelativePathSafeTest(true, "tmp/file");
    isRelativePathSafeTest(false, "/tmp/file");
    isRelativePathSafeTest(false, "../tmp/file");

    isRelativePathSafeTest(true, "filename");
    isRelativePathSafeTest(false, "file*name");
    isRelativePathSafeTest(false, "filename?");
    isRelativePathSafeTest(false, "[filename]");

    isRelativePathSafeTest(true, "package-name-version_3.1.2-x.deb");
    isRelativePathSafeTest(false, "--package-name-version_3.1.2-x.deb");
    isRelativePathSafeTest(false, "~/package-name-version_3.1.2-x.deb");

    isRelativePathSafeTest(true, "directory/windows-installer-3.1.2.msi");
    isRelativePathSafeTest(false, "c:/directory/windows-installer-3.1.2.msi");

    isRelativePathSafeTest(true, "directory\\windows-installer-3.1.2.msi");
    isRelativePathSafeTest(false, "D:\\directory\\windows-installer-3.1.2.msi");
}

} // namespace test
} // namespace utils
} // namespace nx
