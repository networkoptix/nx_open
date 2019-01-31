#include <gtest/gtest.h>

#include <nx/utils/log/log_writers.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/test_options.h>

#include <QtCore/QFile>

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
        m_basePath(lm("%1/log%2").args(
            TestOptions::temporaryDirectoryPath(), nx::utils::random::number()))
    {
    }

    std::unique_ptr<AbstractWriter> makeWriter(size_t size, size_t count = 0)
    {
        File::Settings settings;
        settings.name = m_basePath;
        settings.size = size;
        settings.count = count;
        return std::make_unique<File>(settings);
    }

    void checkFile(const std::vector<QByteArray>& messages = {}, const QByteArray& suffix = {})
    {
        const auto fileName = m_basePath + QString::fromUtf8(suffix + ".log");
        if (messages.empty())
        {
            EXPECT_TRUE(!QFile::exists(fileName));
            return;
        }

        QByteArray expectedContent("\xEF\xBB\xBF", 3);
        for (const auto& m: messages)
            expectedContent += m + kLineSplit;

        QFile file(fileName);
        file.open(QFile::ReadOnly);

        auto actualContent = file.readAll();
        EXPECT_EQ(expectedContent, actualContent);
    }

private:
    QString m_basePath;
};

TEST_F(LogFile, Single)
{
    {
        auto w = makeWriter(20);
        w->write(Level::undefined, lit("1234567890"));
        w->write(Level::undefined, lit("1"));
    }
    checkFile({"1234567890", "1"});

    {
        auto w = makeWriter(20);
        w->write(Level::undefined, lit("7777777")); //<< Overflow.
    }
    checkFile();

    {
        auto w = makeWriter(20);
        w->write(Level::undefined, lit("12345"));
        w->write(Level::undefined, lit("aaa"));
    }
    checkFile({"12345", "aaa"});
}

TEST_F(LogFile, Rotation)
{
    {
        auto w = makeWriter(20, 3);
        w->write(Level::undefined, lit("1234567890"));
    }
    checkFile({"1234567890"});

    {
        auto w = makeWriter(20, 3);
        w->write(Level::undefined, lit("1234567890")); //< Overflow
    }
    checkFile({"1234567890", "1234567890"}, "_001");
    checkFile();

    {
        auto w = makeWriter(20, 3);
        w->write(Level::undefined, lit("xxx"));
        w->write(Level::undefined, lit("yyy"));
    }
    checkFile({"1234567890", "1234567890"}, "_001");
    checkFile({"xxx", "yyy"});

    {
        auto w = makeWriter(20, 3);
        w->write(Level::undefined, lit("12345678901234567890")); // Overflow
        w->write(Level::undefined, lit("1234567890"));
    }
    checkFile({"1234567890", "1234567890"}, "_001");
    checkFile({"xxx", "yyy", "12345678901234567890"}, "_002");
    checkFile({"1234567890"});

    {
        auto w = makeWriter(20, 3);
        w->write(Level::undefined, lit("7777777777")); //< Overflow
        w->write(Level::undefined, lit("a"));
        w->write(Level::undefined, lit("b"));
    }
    checkFile({"1234567890", "1234567890"}, "_001");
    checkFile({"xxx", "yyy", "12345678901234567890"}, "_002");
    checkFile({"1234567890", "7777777777"}, "_003");
    checkFile({"a", "b"});

    {
        auto w = makeWriter(20, 3);
        w->write(Level::undefined, lit("12345678901234567890")); //< Overflow + Rotation
    }
    checkFile({"xxx", "yyy", "12345678901234567890"}, "_001");
    checkFile({"1234567890", "7777777777"}, "_002");
    checkFile({"a", "b", "12345678901234567890"}, "_003");
    checkFile();

    {
        auto w = makeWriter(20, 3);
        w->write(Level::undefined, lit("6666666666"));
        w->write(Level::undefined, lit("1234567890")); //< Overflow
        w->write(Level::undefined, lit("zzz"));
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
