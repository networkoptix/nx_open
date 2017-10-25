#include "merge_test_fixture.h"

#include <nx/utils/test_support/test_options.h>

namespace ec2 {
namespace test {

SystemMergeFixture::SystemMergeFixture()
{
    m_tmpDir = (nx::utils::TestOptions::temporaryDirectoryPath().isEmpty()
        ? QDir::homePath()
        : nx::utils::TestOptions::temporaryDirectoryPath()) + "/merge_test.data";
    QDir(m_tmpDir).removeRecursively();
}

SystemMergeFixture::~SystemMergeFixture()
{
    QDir(m_tmpDir).removeRecursively();
}

bool SystemMergeFixture::initializeSingleServerSystems(int count)
{
    for (int i = 0; i < count; ++i)
    {
        m_servers.emplace_back(
            std::make_unique<PeerWrapper>(lm("%1/peer_%2").args(m_tmpDir, i)));
        if (!m_servers.back()->startAndWaitUntilStarted() ||
            !m_servers.back()->configureAsLocalSystem())
        {
            return false;
        }
    }

    return true;
}

void SystemMergeFixture::whenMergeSystems()
{
    m_prevResult = m_servers.back()->mergeTo(*m_servers.front());
}

void SystemMergeFixture::thenAllServersSynchronizedData()
{
    for (;;)
    {
        if (PeerWrapper::areAllPeersHaveSameTransactionLog(m_servers))
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

QnRestResult::Error SystemMergeFixture::prevMergeResult() const
{
    return m_prevResult;
}

} // namespace test
} // namespace ec2
