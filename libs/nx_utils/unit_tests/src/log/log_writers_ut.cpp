// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>
#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/utils/log/log_writers.h>
#include <nx/utils/log/storage_info.h>
#include <nx/utils/nx_utils_ini.h>
#include <nx/utils/random.h>
#include <nx/utils/test_support/test_options.h>

namespace nx::log::test {

const QByteArray kLineSplit("\n");

class LogFile: public ::testing::Test
{
public:
    // TODO: Use temp directory from TestOptions when available.
    LogFile():
        m_basePath(nx::format("%1/log%2").args(
            TestOptions::temporaryDirectoryPath(), nx::utils::random::number()))
    {
    }

    std::unique_ptr<AbstractWriter> makeWriter(size_t file, size_t volume = kDefaultMaxLogVolumeSizeB, bool archivingEnabled = true)
    {
        File::Settings settings;
        settings.name = m_basePath;
        settings.maxFileSizeB = file;
        settings.maxVolumeSizeB = volume;
        settings.archivingEnabled = archivingEnabled;
        return std::make_unique<File>(settings);
    }

    void checkFile(
        const std::vector<QByteArray>& messages = {},
        const QString& suffix = {},
        const File::Extension& extension = File::Extension::log)
    {
        const auto fileName = m_basePath + suffix + toQString(extension);
        if (messages.empty())
        {
            EXPECT_TRUE(!QFile::exists(fileName));
            return;
        }

        QByteArray expectedContent("\xEF\xBB\xBF", 3);
        for (const auto& m: messages)
            expectedContent += m + kLineSplit;

        QByteArray actualContent;
        if (extension == File::Extension::log)
        {
            ASSERT_TRUE(QFile::exists(fileName));
            QFile file(fileName);
            ASSERT_TRUE(file.open(QFile::ReadOnly));
            actualContent = file.readAll();
        }
        else
        {
            ASSERT_TRUE(QFile::exists(fileName));
            QuaZip archive(fileName);
            ASSERT_TRUE(archive.open(QuaZip::Mode::mdUnzip));
            QuaZipFile file(&archive);
            ASSERT_TRUE(archive.goToFirstFile());
            ASSERT_TRUE(file.open(QIODevice::ReadOnly));
            actualContent = file.readAll();
        }

        EXPECT_EQ(expectedContent, actualContent);
    }

    virtual void SetUp() override
    {
        auto dir = QFileInfo(m_basePath).dir();
        const auto logs = dir.entryList(
            {
                QString("*") + toQString(File::Extension::log),
                QString("*") + toQString(File::Extension::zip)
            },
            QDir::Files, QDir::Name);
        for (const auto& log: logs)
            dir.remove(log);

        nx::log::detail::setVolumeInfoGetter(nullptr);
    }

    qint64 bytesWritten()
    {
        qint64 total = 0;
        auto dir = QFileInfo(m_basePath).dir();
        const auto logs = dir.entryInfoList(
            {
                QString("*") + toQString(File::Extension::log),
                QString("*") + toQString(File::Extension::zip)
            },
            QDir::Files, QDir::Name);
        for (const auto& log: logs)
            total += log.size();

        return total;
    }

protected:
    QString m_basePath;
};

TEST_F(LogFile, Single)
{
    static constexpr size_t kLogSize = 20;
    {
        auto w = makeWriter(kLogSize);
        w->write(Level::undefined, "1234567890");
        w->write(Level::undefined, "1");
    }
    checkFile({"1234567890", "1"});

    {
        auto w = makeWriter(kLogSize);
        w->write(Level::undefined, "7777777"); //<< Overflow.
    }
    checkFile();

    {
        auto w = makeWriter(kLogSize);
        w->write(Level::undefined, "12345");
        w->write(Level::undefined, "aaa");
    }
    checkFile({"12345", "aaa"});
}

TEST_F(LogFile, Rotation)
{
    static constexpr size_t kLogSize = 20;
    static constexpr size_t kVolumeSize = 500;
    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "1234567890");
    }
    checkFile({"1234567890"});

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "1234567890"); //< Overflow
    }
    checkFile({"1234567890", "1234567890"}, "_001", File::Extension::zip);
    checkFile();

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "xxx");
        w->write(Level::undefined, "yyy");
    }
    checkFile({"1234567890", "1234567890"}, "_001", File::Extension::zip);
    checkFile({"xxx", "yyy"});

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "12345678901234567890"); // Overflow
        w->write(Level::undefined, "1234567890");
    }
    checkFile({"1234567890", "1234567890"}, "_001", File::Extension::zip);
    checkFile({"xxx", "yyy", "12345678901234567890"}, "_002", File::Extension::zip);
    checkFile({"1234567890"});

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "7777777777"); //< Overflow
        w->write(Level::undefined, "a");
        w->write(Level::undefined, "b");
    }
    checkFile({"1234567890", "1234567890"}, "_001", File::Extension::zip);
    checkFile({"xxx", "yyy", "12345678901234567890"}, "_002", File::Extension::zip);
    checkFile({"1234567890", "7777777777"}, "_003", File::Extension::zip);
    checkFile({"a", "b"});

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "12345678901234567890"); //< Overflow + Rotation
    }
    checkFile({"xxx", "yyy", "12345678901234567890"}, "_001", File::Extension::zip);
    checkFile({"1234567890", "7777777777"}, "_002", File::Extension::zip);
    checkFile({"a", "b", "12345678901234567890"}, "_003", File::Extension::zip);
    checkFile();

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "6666666666");
        w->write(Level::undefined, "1234567890"); //< Overflow + Rotation
        w->write(Level::undefined, "zzz");
    }
    checkFile({"1234567890", "7777777777"}, "_001", File::Extension::zip);
    checkFile({"a", "b", "12345678901234567890"}, "_002", File::Extension::zip);
    checkFile({"6666666666", "1234567890"}, "_003", File::Extension::zip);
    checkFile({"zzz"});
}

TEST_F(LogFile, RotationWithArchiveOpenedByAnotherProcess)
{
    static constexpr size_t kLogSize = 20;
    static constexpr size_t kVolumeSize = 500;
    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "1234567890");
        w->write(Level::undefined, "1234567890"); //< Overflow
        w->write(Level::undefined, "xxx");
        w->write(Level::undefined, "yyy");
        w->write(Level::undefined, "12345678901234567890"); //< Overflow
        w->write(Level::undefined, "7777777777");
        w->write(Level::undefined, "8888888888"); //< Overflow
        w->write(Level::undefined, "a");
        w->write(Level::undefined, "b");
    }
    checkFile({"1234567890", "1234567890"}, "_001", File::Extension::zip);
    checkFile({"xxx", "yyy", "12345678901234567890"}, "_002", File::Extension::zip);
    checkFile({"7777777777", "8888888888"}, "_003", File::Extension::zip);
    checkFile({"a", "b"});

    // On Windows an opened file can be neither removed nor renamed.
    QFile openedArchive(m_basePath + "_002" + toQString(File::Extension::zip));
    ASSERT_TRUE(openedArchive.open(QIODevice::ReadOnly));
    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "12345678901234567890"); //< Overflow + Rotation
    }
    openedArchive.close();

#if defined(Q_OS_WIN)
    // The opened archive keeps its number, the others are numbered around it.
    checkFile({"7777777777", "8888888888"}, "_001", File::Extension::zip);
    checkFile({"xxx", "yyy", "12345678901234567890"}, "_002", File::Extension::zip);
    checkFile({"a", "b", "12345678901234567890"}, "_003", File::Extension::zip);
#else
    checkFile({"xxx", "yyy", "12345678901234567890"}, "_001", File::Extension::zip);
    checkFile({"7777777777", "8888888888"}, "_002", File::Extension::zip);
    checkFile({"a", "b", "12345678901234567890"}, "_003", File::Extension::zip);
#endif
    checkFile();
}

TEST_F(LogFile, RotationWithMaxArchivesAndOldestOpenedByAnotherProcess)
{
    static constexpr size_t kLogSize = 20;
    const auto archiveName = [this](int number)
    {
        return File::makeFileName(m_basePath, number, File::Extension::zip);
    };

    for (int i = 1; i <= kMaxLogRotation; ++i)
    {
        QFile archive(archiveName(i));
        ASSERT_TRUE(archive.open(QIODevice::WriteOnly));
    }

    // On Windows an opened file can not be removed, so the next oldest one has to be.
    QFile openedArchive(archiveName(1));
    ASSERT_TRUE(openedArchive.open(QIODevice::ReadOnly));
    {
        auto w = makeWriter(kLogSize);
        w->write(Level::undefined, "1234567890");
        w->write(Level::undefined, "1234567890"); //< Overflow + Rotation
    }
    openedArchive.close();

    EXPECT_FALSE(QFile::exists(archiveName(kMaxLogRotation + 1)));
    checkFile({"1234567890", "1234567890"}, "_999", File::Extension::zip);
    const QFileInfo baseInfo(m_basePath);
    const auto archives = baseInfo.dir().entryList(
        {baseInfo.fileName() + "_*" + toQString(File::Extension::zip)}, QDir::Files);
    EXPECT_EQ(kMaxLogRotation, archives.size());
    checkFile();
}

class LogFileWithOccupiedName: public LogFile
{
protected:
    /** Writes 3 archives and the current log file. */
    void SetUp() override
    {
        LogFile::SetUp();
        {
            auto w = makeWriter(kLogSize, kVolumeSize);
            w->write(Level::undefined, "1234567890");
            w->write(Level::undefined, "1234567890"); //< Overflow
            w->write(Level::undefined, "xxx");
            w->write(Level::undefined, "yyy");
            w->write(Level::undefined, "12345678901234567890"); //< Overflow
            w->write(Level::undefined, "7777777777");
            w->write(Level::undefined, "8888888888"); //< Overflow
            w->write(Level::undefined, "a");
            w->write(Level::undefined, "b");
        }
    }

    void TearDown() override
    {
        if (!m_occupiedName.isEmpty())
            QDir().rmdir(m_occupiedName);
    }

    /**
     * Replaces the first archive with a directory with the given name: renaming to this name fails
     * on every platform.
     */
    void occupy(const QString& name)
    {
        ASSERT_TRUE(QFile::remove(m_basePath + "_001" + toQString(File::Extension::zip)));
        m_occupiedName = name;
        ASSERT_TRUE(QDir().mkdir(m_occupiedName));
    }

    static constexpr size_t kLogSize = 20;
    static constexpr size_t kVolumeSize = 500;
    QString m_occupiedName;
};

TEST_F(LogFileWithOccupiedName, Rotation)
{
    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        // Occupied after construction, so only the rotation has to deal with it.
        occupy(m_basePath + "_001" + toQString(File::Extension::zip));
        w->write(Level::undefined, "12345678901234567890"); //< Overflow + Rotation
    }

    EXPECT_TRUE(QFileInfo(m_occupiedName).isDir());
    checkFile({"xxx", "yyy", "12345678901234567890"}, "_002", File::Extension::zip);
    checkFile({"7777777777", "8888888888"}, "_003", File::Extension::zip);
    checkFile({"a", "b", "12345678901234567890"}, "_004", File::Extension::zip);
    checkFile();
}

TEST_F(LogFileWithOccupiedName, LeftoversRotation)
{
    // The archive _002 can not be moved to its temporary name _001, as if it were opened by
    // another process on Windows.
    occupy(m_basePath + "_001" + toQString(File::Extension::zipTmp));
    makeWriter(kLogSize, kVolumeSize);

    checkFile({}, "_001", File::Extension::zip);
    checkFile({"xxx", "yyy", "12345678901234567890"}, "_002", File::Extension::zip);
    checkFile({"7777777777", "8888888888"}, "_003", File::Extension::zip);
    checkFile({"a", "b"});
}

TEST_F(LogFile, RotationZipNoZip)
{
    static constexpr size_t kLogSize = 20;
    static constexpr size_t kVolumeSize = 500;
    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "1234567890");
    }
    checkFile({"1234567890"});

    {
        auto w = makeWriter(kLogSize, kVolumeSize, false);
        w->write(Level::undefined, "1234567890"); //< Overflow
    }
    checkFile({"1234567890", "1234567890"}, "_001", File::Extension::log);
    checkFile();

    {
        auto w = makeWriter(kLogSize, kVolumeSize); //< Zip _001.log
        w->write(Level::undefined, "xxx");
        w->write(Level::undefined, "yyy");
        w->write(Level::undefined, "2222222222"); //< Overflow
    }
    checkFile({"1234567890", "1234567890"}, "_001", File::Extension::zip);
    checkFile({"xxx", "yyy", "2222222222"}, "_002", File::Extension::zip);
    checkFile();

    {
        auto w = makeWriter(kLogSize, kVolumeSize, false);
        w->write(Level::undefined, "a");
        w->write(Level::undefined, "b");
        w->write(Level::undefined, "c");
        w->write(Level::undefined, "d");
        w->write(Level::undefined, "3333333333"); //< Overflow
        w->write(Level::undefined, "aaaaaa");
        w->write(Level::undefined, "bbbbbb");
        w->write(Level::undefined, "cccccc"); //< Overflow
        w->write(Level::undefined, "dddddd");
        w->write(Level::undefined, "aaaaaaaaa"); //< Overflow
        w->write(Level::undefined, "bbbbbbbbb");
        w->write(Level::undefined, "ccccccccc"); //< Overflow + Rotation
        w->write(Level::undefined, "ddddddddd");
    }
    checkFile({"xxx", "yyy", "2222222222"}, "_001", File::Extension::zip);
    checkFile({"a", "b", "c", "d", "3333333333"}, "_002", File::Extension::log);
    checkFile({"aaaaaa", "bbbbbb", "cccccc"}, "_003", File::Extension::log);
    checkFile({"dddddd", "aaaaaaaaa"}, "_004", File::Extension::log);
    checkFile({"bbbbbbbbb", "ccccccccc"}, "_005", File::Extension::log);
    checkFile({"ddddddddd"});

    {
        auto w = makeWriter(kLogSize, kVolumeSize); //< Zip all text logs
        w->write(Level::undefined, "111");
    }
    checkFile({"xxx", "yyy", "2222222222"}, "_001", File::Extension::zip);
    checkFile({"a", "b", "c", "d", "3333333333"}, "_002", File::Extension::zip);
    checkFile({"aaaaaa", "bbbbbb", "cccccc"}, "_003", File::Extension::zip);
    checkFile({"dddddd", "aaaaaaaaa"}, "_004", File::Extension::zip);
    checkFile({"bbbbbbbbb", "ccccccccc"}, "_005", File::Extension::zip);
    checkFile({"ddddddddd", "111"});
}

TEST_F(LogFile, RotationByFreeSpace)
{
    static constexpr size_t kLogSize = 20;
    static constexpr qint64 kMiB = 1024 * 1024;
    static constexpr size_t kVolumeSize = 1024 * kMiB;
    const auto kReservedFreeSpace = nx::utils::ini().logReservedVolumeSizeMB * kMiB;
    const qint64 kTotalVolumeSize = 100 * 1024 * kMiB; // to make percentage limit bigger.

    nx::log::detail::setVolumeInfoGetter(
        [&](auto /*path*/) -> std::optional<nx::log::detail::VolumeInfo>
        {
            return std::make_tuple(kTotalVolumeSize, kReservedFreeSpace + 500 - bytesWritten());
        }
    );

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "1234567890");
    }
    checkFile({"1234567890"});

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "1234567890"); //< Overflow
    }
    checkFile({"1234567890", "1234567890"}, "_001", File::Extension::zip);
    checkFile();

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "xxx");
        w->write(Level::undefined, "yyy");
    }
    checkFile({"1234567890", "1234567890"}, "_001", File::Extension::zip);
    checkFile({"xxx", "yyy"});

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "12345678901234567890"); // Overflow
        w->write(Level::undefined, "1234567890");
    }
    checkFile({"1234567890", "1234567890"}, "_001", File::Extension::zip);
    checkFile({"xxx", "yyy", "12345678901234567890"}, "_002", File::Extension::zip);
    checkFile({"1234567890"});

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "7777777777"); //< Overflow
        w->write(Level::undefined, "a");
        w->write(Level::undefined, "b");
    }
    checkFile({"1234567890", "1234567890"}, "_001", File::Extension::zip);
    checkFile({"xxx", "yyy", "12345678901234567890"}, "_002", File::Extension::zip);
    checkFile({"1234567890", "7777777777"}, "_003", File::Extension::zip);
    checkFile({"a", "b"});

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "12345678901234567890"); //< Overflow + Rotation
    }
    checkFile({"xxx", "yyy", "12345678901234567890"}, "_001", File::Extension::zip);
    checkFile({"1234567890", "7777777777"}, "_002", File::Extension::zip);
    checkFile({"a", "b", "12345678901234567890"}, "_003", File::Extension::zip);
    checkFile();

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "6666666666");
        w->write(Level::undefined, "1234567890"); //< Overflow + Rotation
        w->write(Level::undefined, "zzz");
    }
    checkFile({"1234567890", "7777777777"}, "_001", File::Extension::zip);
    checkFile({"a", "b", "12345678901234567890"}, "_002", File::Extension::zip);
    checkFile({"6666666666", "1234567890"}, "_003", File::Extension::zip);
    checkFile({"zzz"});

    // No free space, required count of bytes to remove is bigger than
    // all written logs. All logs have to be removed.
    nx::log::detail::setVolumeInfoGetter(
        [&](auto /*path*/) -> std::optional<nx::log::detail::VolumeInfo>
        {
            return std::make_tuple(kTotalVolumeSize, 0);
        }
    );
    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "6666666666");
        w->write(Level::undefined, "1234567890"); //< Overflow + Rotation
    }
    checkFile({}, "_001", File::Extension::zip);
    checkFile({}, "_002", File::Extension::zip);
    checkFile({}, "_003", File::Extension::zip);
    checkFile();
}

} // namespace nx::log::test
