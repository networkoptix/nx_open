#include "merge_test_fixture.h"

#include <stdexcept>

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

void SystemMergeFixture::addSetting(const std::string& name, const std::string& value)
{
    m_settings.emplace(name, value);
}

bool SystemMergeFixture::initializeSingleServerSystems(int count)
{
    for (int i = 0; i < count; ++i)
    {
        m_servers.emplace_back(
            std::make_unique<PeerWrapper>(lm("%1/peer_%2").args(m_tmpDir, m_servers.size())));
        for (const auto& nameAndValue: m_settings)
            m_servers.back()->addSetting(nameAndValue.first, nameAndValue.second);
        if (!m_servers.back()->startAndWaitUntilStarted() ||
            !m_servers.back()->configureAsLocalSystem())
        {
            return false;
        }
    }

    return true;
}

int SystemMergeFixture::peerCount() const
{
    return (int) m_servers.size();
}

const PeerWrapper& SystemMergeFixture::peer(int index) const
{
    return *m_servers[index];
}

PeerWrapper& SystemMergeFixture::peer(int index)
{
    return *m_servers[index];
}

void SystemMergeFixture::mergeSystems()
{
    m_prevResult = m_servers.back()->mergeTo(*m_servers.front());
}

void SystemMergeFixture::waitUntilAllServersAreInterconnected()
{
    for (;;)
    {
        if (PeerWrapper::arePeersInterconnected(m_servers))
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void SystemMergeFixture::waitUntilAllServersSynchronizedData()
{
    for (;;)
    {
        if (PeerWrapper::areAllPeersHaveSameTransactionLog(m_servers))
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

ApiSystemMergeHistoryRecordList SystemMergeFixture::waitUntilMergeHistoryIsAdded()
{
    ApiSystemMergeHistoryRecordList mergeHistory;
    for (;;)
    {
        const auto errorCode =
            peer(0).ecConnection()->getMiscManager(Qn::kSystemAccess)->
                getSystemMergeHistorySync(&mergeHistory);
        if (errorCode != ErrorCode::ok)
            throw std::runtime_error(toString(errorCode).toStdString());
        if (!mergeHistory.empty())
            return mergeHistory;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

QnRestResult::Error SystemMergeFixture::prevMergeResult() const
{
    return m_prevResult;
}

} // namespace test
} // namespace ec2
