#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QFile>

#include <nx/utils/file_system.h>

static const QString kTestDirectoryName("__tmp_fs_test");
static const QString kSourceDirectoryName("src");
static const QString kTargetDirectoryName("dst");

namespace nx {
namespace utils {
namespace test {

class CopyTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        QDir rootDir(QDir::temp().absoluteFilePath(kTestDirectoryName));

        rootDir.removeRecursively();
        ASSERT_TRUE(QDir().mkdir(rootDir.absolutePath()));

        ASSERT_TRUE(rootDir.mkdir(kSourceDirectoryName));
        sourceDir = rootDir.absoluteFilePath(kSourceDirectoryName);
        ASSERT_TRUE(rootDir.mkdir(kTargetDirectoryName));
        targetDir = rootDir.absoluteFilePath(kTargetDirectoryName);
    }

    virtual void TearDown() override
    {
        QDir rootDir(QDir::temp().absoluteFilePath(kTestDirectoryName));
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

TEST_F(CopyTest, ensureDirectory)
{
    const QDir dir(sourceDir.absoluteFilePath("dir/subdir"));
    ASSERT_TRUE(file_system::ensureDir(dir));

    ASSERT_TRUE(dir.exists());
    ASSERT_TRUE(QFileInfo(dir.absolutePath()).isDir());
}

TEST_F(CopyTest, copyFile)
{
    const QString fileName("file");

    const auto srcName = createFile(fileName);
    ASSERT_FALSE(srcName.isEmpty());

    auto result = file_system::copy(srcName, targetDir.absoluteFilePath(fileName));
    ASSERT_EQ(result.code, file_system::Result::ok);
    ASSERT_TRUE(QFileInfo(targetDir.filePath(fileName)).exists());
}

TEST_F(CopyTest, copyFileToDirectory)
{
    const QString fileName("file");

    const auto srcName = createFile(fileName);
    ASSERT_FALSE(srcName.isEmpty());

    auto result = file_system::copy(srcName, targetDir.absolutePath());
    ASSERT_EQ(result.code, file_system::Result::ok);
    ASSERT_TRUE(QFileInfo(targetDir.filePath(fileName)).exists());
}

TEST_F(CopyTest, copyFileToNotExistingDirectory)
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

TEST_F(CopyTest, copyFileOverwrite)
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

TEST_F(CopyTest, copyEmptyDirectory)
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

TEST_F(CopyTest, copyDirectory)
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

TEST_F(CopyTest, copyDirectoryOverwrite)
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

#if defined(Q_OS_UNIX)
    TEST_F(CopyTest, readSymLinkTarget)
    {
        const QString linkName("link");

        createFile("file");

        const auto srcName = createSymLink(linkName, "file");
        ASSERT_FALSE(srcName.isEmpty());

        QString symLinkTarget = file_system::symLinkTarget(srcName);
        ASSERT_EQ(symLinkTarget, "file");
    }

    TEST_F(CopyTest, copyFileLink)
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

    TEST_F(CopyTest, copyDirectoryLink)
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

    TEST_F(CopyTest, followSymLinks)
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

    TEST_F(CopyTest, symLinkOverlap)
    {
        const auto srcFile = createFile("file");
        ASSERT_FALSE(srcFile.isEmpty());
        const auto srcLink = createSymLink("link", "file");
        ASSERT_FALSE(srcLink.isEmpty());

        auto result = file_system::copy(srcLink, srcFile);
        ASSERT_EQ(result.code, file_system::Result::sourceAndTargetAreSame);
    }

    TEST_F(CopyTest, directorySelfOverlap)
    {
        createPath("dir/dir/dir");
        createSymLink("dir/dir/dir/dir", "../dir");

        auto result = file_system::copy(
            sourceDir.absoluteFilePath("dir/dir"), sourceDir.absolutePath());
        ASSERT_EQ(result.code, file_system::Result::sourceAndTargetAreSame);
    }

#endif //defined(Q_OS_UNIX)

TEST_F(CopyTest, directoryOverlap)
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


TEST_F(CopyTest, simpleOverlap)
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

static void isSafeRelativePathTest(bool expectedResult, const char* path)
{
    EXPECT_EQ(expectedResult, file_system::isSafeRelativePath(path)) << path;
}

TEST(FileSystem, isSafeRelativePath)
{
    isSafeRelativePathTest(true, "tmp/file");
    isSafeRelativePathTest(false, "/tmp/file");
    isSafeRelativePathTest(false, "../tmp/file");

    isSafeRelativePathTest(true, "filename");
    isSafeRelativePathTest(false, "file*name");
    isSafeRelativePathTest(false, "filename?");
    isSafeRelativePathTest(false, "[filename]");

    isSafeRelativePathTest(true, "package-name-version_3.1.2-x.deb");
    isSafeRelativePathTest(false, "--package-name-version_3.1.2-x.deb");
    isSafeRelativePathTest(false, "~/package-name-version_3.1.2-x.deb");

    isSafeRelativePathTest(true, "directory/windows-installer-3.1.2.msi");
    isSafeRelativePathTest(false, "c:/directory/windows-installer-3.1.2.msi");

    isSafeRelativePathTest(true, "directory\\windows-installer-3.1.2.msi");
    isSafeRelativePathTest(false, "D:\\directory\\windows-installer-3.1.2.msi");
}

} // namespace test
} // namespace utils
} // namespace nx
