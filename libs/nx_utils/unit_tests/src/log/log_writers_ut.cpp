// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/log/log_writers.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/test_options.h>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

namespace nx {
namespace utils {
namespace log {
namespace test {

#if defined(Q_OS_WIN)
    const QByteArray kLineSplit("\r\n");
#else
    const QByteArray kLineSplit("\n");
#endif

class LogFile: public ::testing::Test
{
public:
    // TODO: Use temp directory from TestOptions when avaliable.
    LogFile():
        m_basePath(nx::format("%1/log%2").args(
            TestOptions::temporaryDirectoryPath(), nx::utils::random::number()))
    {
    }

    std::unique_ptr<AbstractWriter> makeWriter(size_t file, size_t volume = kDefaultMaxLogVolumeSizeB)
    {
        File::Settings settings;
        settings.name = m_basePath;
        settings.maxFileSizeB = file;
        settings.maxVolumeSizeB = volume;
        return std::make_unique<File>(settings);
    }

    void checkFile(const std::vector<QByteArray>& messages = {}, const QByteArray& suffix = {})
    {
        const auto fileName = m_basePath + QString::fromUtf8(suffix.isEmpty()
            ? QByteArray(File::kExtensionWithSeparator) : (suffix + File::kRotateExtensionWithSeparator));
        if (messages.empty())
        {
            EXPECT_TRUE(!QFile::exists(fileName));
            return;
        }

        QByteArray expectedContent("\xEF\xBB\xBF", 3);
        for (const auto& m: messages)
            expectedContent += m + kLineSplit;

        QByteArray actualContent;
        if (suffix.isEmpty())
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
                QString("*") + File::kExtensionWithSeparator,
                QString("*") + File::kRotateExtensionWithSeparator
            },
            QDir::Files, QDir::Name);
        for (auto log: logs)
            dir.remove(log);
    }

private:
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
    checkFile({"1234567890", "1234567890"}, "_001");
    checkFile();

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "xxx");
        w->write(Level::undefined, "yyy");
    }
    checkFile({"1234567890", "1234567890"}, "_001");
    checkFile({"xxx", "yyy"});

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "12345678901234567890"); // Overflow
        w->write(Level::undefined, "1234567890");
    }
    checkFile({"1234567890", "1234567890"}, "_001");
    checkFile({"xxx", "yyy", "12345678901234567890"}, "_002");
    checkFile({"1234567890"});

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "7777777777"); //< Overflow
        w->write(Level::undefined, "a");
        w->write(Level::undefined, "b");
    }
    checkFile({"1234567890", "1234567890"}, "_001");
    checkFile({"xxx", "yyy", "12345678901234567890"}, "_002");
    checkFile({"1234567890", "7777777777"}, "_003");
    checkFile({"a", "b"});

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "12345678901234567890"); //< Overflow + Rotation
    }
    checkFile({"xxx", "yyy", "12345678901234567890"}, "_001");
    checkFile({"1234567890", "7777777777"}, "_002");
    checkFile({"a", "b", "12345678901234567890"}, "_003");
    checkFile();

    {
        auto w = makeWriter(kLogSize, kVolumeSize);
        w->write(Level::undefined, "6666666666");
        w->write(Level::undefined, "1234567890"); //< Overflow + Rotation
        w->write(Level::undefined, "zzz");
    }
    checkFile({"1234567890", "7777777777"}, "_001");
    checkFile({"a", "b", "12345678901234567890"}, "_002");
    checkFile({"6666666666", "1234567890"}, "_003");
    checkFile({"zzz"});
}

} // namespace test
} // namespace log
} // namespace utils
} // namespace nx
