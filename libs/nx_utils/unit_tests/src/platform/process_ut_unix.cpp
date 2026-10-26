// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include <QtCore/QProcess>

#include <nx/utils/platform/process.h>

namespace nx::utils::test {

/**
 * A process started by nx::utils::startProcess() / startProcessDetached() must not inherit
 * descriptors of this process. An inherited descriptor keeps the kernel object alive after this
 * process has closed it, as long as the child lives.
 */
class Process: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        // The write end is intentionally inheritable (no FD_CLOEXEC).
        ASSERT_EQ(0, ::pipe(m_pipe));
    }

    virtual void TearDown() override
    {
        if (m_process)
        {
            m_process->kill();
            m_process->waitForFinished(5000);
        }

        if (m_detachedPid > 0)
        {
            ::kill(m_detachedPid, SIGKILL);
            ::waitpid(m_detachedPid, nullptr, 0);
        }

        for (int fd: m_pipe)
        {
            if (fd >= 0)
                ::close(fd);
        }
    }

    void whenStartLongLivingProcess()
    {
        m_process = startProcess(kLongLivingProgram, kLongLivingProgramArguments);
        ASSERT_TRUE(m_process->waitForStarted(5000)) << m_process->errorString().toStdString();
    }

    void whenStartLongLivingDetachedProcess()
    {
        ASSERT_TRUE(startProcessDetached(
            kLongLivingProgram, kLongLivingProgramArguments, QString(), &m_detachedPid));
        ASSERT_GT(m_detachedPid, 0);
    }

    void andCloseWriteEndOfThePipe()
    {
        ::close(m_pipe[1]);
        m_pipe[1] = -1;
    }

    void thenReadEndReportsEofImmediately()
    {
        // If the child had inherited the write end, EOF would be reported only after the child
        // exit, i.e. poll() would time out.
        pollfd pfd{};
        pfd.fd = m_pipe[0];
        pfd.events = POLLIN;
        ASSERT_GT(::poll(&pfd, 1, 3000), 0) << "The write end of the pipe is held by the child";

        char buf[1];
        ASSERT_EQ(0, ::read(m_pipe[0], buf, sizeof(buf)));
    }

private:
    static constexpr char kLongLivingProgram[] = "/bin/sleep";
    inline static const QStringList kLongLivingProgramArguments{"60"};

    int m_pipe[2] = {-1, -1};
    std::unique_ptr<QProcess> m_process;
    qint64 m_detachedPid = 0;
};

TEST_F(Process, started_process_does_not_inherit_descriptors)
{
    whenStartLongLivingProcess();
    andCloseWriteEndOfThePipe();

    thenReadEndReportsEofImmediately();
}

TEST_F(Process, detached_process_does_not_inherit_descriptors)
{
    whenStartLongLivingDetachedProcess();
    andCloseWriteEndOfThePipe();

    thenReadEndReportsEofImmediately();
}

} // namespace nx::utils::test
