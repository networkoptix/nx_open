// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <fstream>

#include <gtest/gtest.h>

#include <nx/utils/fs/file.h>
#include <nx/utils/fs/async_file_processor.h>
#include <nx/utils/random.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::utils::fs::test {

class File:
    public utils::test::TestWithTemporaryDirectory,
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_resourceFilePath = ":/test_resource_file.txt";
        QFile qFile(m_resourceFilePath.c_str());
        ASSERT_TRUE(qFile.open(QIODevice::ReadOnly));
        m_fileContents = qFile.readAll().toStdString();

        m_filePath = testDataDir().toStdString() + "/test.dat";

        std::ofstream f(m_filePath, std::ios_base::out | std::ios_base::binary);
        ASSERT_TRUE(f.is_open());
        f << m_fileContents;
    }

    void givenOpenedFile()
    {
        whenOpenFileAsync();
        thenOpenSucceeded();
    }

    void givenOpenedQtResourceFile()
    {
        m_file = std::make_unique<nx::utils::fs::File>(m_resourceFilePath);
        ASSERT_TRUE(m_file->open(QIODevice::ReadOnly));
    }

    void givenOpenedRegularFile()
    {
        m_file = std::make_unique<nx::utils::fs::File>(m_filePath);
        ASSERT_TRUE(m_file->open(QIODevice::ReadOnly));
    }

    void whenStatAsync()
    {
        m_scheduler.stat(
            m_filePath,
            [this](auto&&... args) { m_statResults.emplace(std::forward<decltype(args)>(args)...); });
    }

    void whenOpenFileAsync()
    {
        m_file = std::make_unique<nx::utils::fs::File>(m_filePath);
        m_file->openAsync(
            &m_scheduler, QIODevice::ReadOnly, 0,
            [this](auto result) { m_openResults.push(result); });
    }

    void whenReadWholeFileAsync()
    {
        m_file->readAllAsync(
            &m_scheduler,
            [this](auto&&... args) { m_readAllResults.emplace(std::forward<decltype(args)>(args)...); });
    }

    void whenReadWholeFileGradually()
    {
        static constexpr int kReadSize = 1;

        m_readBuffer.reserve(m_readBuffer.size() + kReadSize);

        m_file->readAsync(
            &m_scheduler,
            &m_readBuffer,
            [this](SystemError::ErrorCode sysErrorCode, std::size_t bytesRead)
            {
                if (sysErrorCode != SystemError::noError)
                    return m_readAllResults.emplace(sysErrorCode, nx::Buffer());

                if (bytesRead == 0)
                {
                    // EOF
                    return m_readAllResults.emplace(
                        sysErrorCode,
                        std::exchange(m_readBuffer, nx::Buffer()));
                }

                // Continuing reading file.
                whenReadWholeFileGradually();
            });
    }

    void whenStatQtResourceFile()
    {
        whenStatFile(m_resourceFilePath);
    }

    void whenStatRegularFile()
    {
        whenStatFile(m_filePath);
    }

    void whenStatFile(const std::string& path)
    {
        if (auto fileStat = nx::utils::fs::stat(path); fileStat)
            m_statResults.emplace(SystemError::noError, *fileStat);
        else
            m_statResults.emplace(SystemError::getLastOSErrorCode(), FileStat());
    }

    void whenReadWholeFile()
    {
        if (auto data = m_file->readAll(); data)
            m_readAllResults.emplace(SystemError::noError, std::move(*data));
        else
            m_readAllResults.emplace(SystemError::getLastOSErrorCode(), nx::Buffer());
    }

    void thenStatSucceeded()
    {
        m_lastStatResult = m_statResults.pop();
        ASSERT_EQ(SystemError::noError, std::get<0>(*m_lastStatResult));
    }

    void andCorrectSizeHasBeenReported()
    {
        ASSERT_EQ(m_fileContents.size(), std::get<1>(*m_lastStatResult).st_size);
    }

    void thenOpenSucceeded()
    {
        const auto result = m_openResults.pop();
        ASSERT_EQ(SystemError::noError, result);
    }

    void thenReadSucceeded()
    {
        m_lastReadAllResult = m_readAllResults.pop();
        ASSERT_EQ(SystemError::noError, std::get<0>(*m_lastReadAllResult));
    }

    void andTheWholeFileWasRead()
    {
        ASSERT_EQ(m_fileContents, std::get<1>(*m_lastReadAllResult).toStdString());
    }

private:
    using ReadAllResult = std::tuple<SystemError::ErrorCode, nx::Buffer>;
    using StatResult = std::tuple<SystemError::ErrorCode, nx::utils::fs::FileStat>;

    nx::utils::fs::FileAsyncIoScheduler m_scheduler;
    std::unique_ptr<nx::utils::fs::File> m_file;
    std::string m_resourceFilePath;
    std::string m_filePath;
    std::string m_fileContents;
    nx::Buffer m_readBuffer;

    nx::utils::SyncQueue<SystemError::ErrorCode> m_openResults;

    nx::utils::SyncQueue<ReadAllResult> m_readAllResults;
    std::optional<ReadAllResult> m_lastReadAllResult;

    nx::utils::SyncQueue<StatResult> m_statResults;
    std::optional<StatResult> m_lastStatResult;
};

TEST_F(File, stat_supports_qt_resource_files)
{
    whenStatQtResourceFile();

    thenStatSucceeded();
    andCorrectSizeHasBeenReported();
}

TEST_F(File, stat_supports_regular_files)
{
    whenStatRegularFile();

    thenStatSucceeded();
    andCorrectSizeHasBeenReported();
}

TEST_F(File, read_qt_resource_files)
{
    givenOpenedQtResourceFile();

    whenReadWholeFile();

    thenReadSucceeded();
    andTheWholeFileWasRead();
}

TEST_F(File, read_regular_files)
{
    givenOpenedRegularFile();

    whenReadWholeFile();

    thenReadSucceeded();
    andTheWholeFileWasRead();
}

TEST_F(File, async_stat)
{
    whenStatAsync();

    thenStatSucceeded();
    andCorrectSizeHasBeenReported();
}

TEST_F(File, async_stat_missing_file)
{
    whenStatAsync();

    thenStatSucceeded();
    andCorrectSizeHasBeenReported();
}

TEST_F(File, async_read_file_gradually)
{
    givenOpenedFile();

    whenReadWholeFileGradually();

    thenReadSucceeded();
    andTheWholeFileWasRead();
}

TEST_F(File, async_read_all)
{
    givenOpenedFile();

    whenReadWholeFileAsync();

    thenReadSucceeded();
    andTheWholeFileWasRead();
}

} // namespace nx::utils::fs::test
