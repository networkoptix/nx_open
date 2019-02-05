#include "merge_test_fixture.h"

#include <stdexcept>

#include <nx/utils/test_support/test_options.h>

namespace ec2 {
namespace test {

SystemMergeFixture::SystemMergeFixture(const std::string& baseDirectory):
    m_baseDirectory(baseDirectory)
{
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
            std::make_unique<PeerWrapper>(lm("%1/peer_%2")
                .args(m_baseDirectory, m_servers.size())));
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

QnRestResult::Error SystemMergeFixture::mergePeers(
    PeerWrapper& what,
    PeerWrapper& to,
    std::vector<MergeAttributes::Attribute> attributes)
{
    m_prevResult = what.mergeTo(to, std::move(attributes));
    return m_prevResult;
}

QnRestResult::Error SystemMergeFixture::mergeSystems()
{
    m_prevResult = m_servers.back()->mergeTo(*m_servers.front());
    return m_prevResult;
}

void SystemMergeFixture::waitUntilAllServersAreInterconnected()
{
    for (;;)
    {
        if (PeerWrapper::peersInterconnected(m_servers))
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void SystemMergeFixture::waitUntilAllServersSynchronizedData()
{
    for (;;)
    {
        if (PeerWrapper::allPeersHaveSameTransactionLog(m_servers))
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

nx::vms::api::SystemMergeHistoryRecordList SystemMergeFixture::waitUntilMergeHistoryIsAdded()
{
    nx::vms::api::SystemMergeHistoryRecordList mergeHistory;
    for (;;)
    {
        const auto errorCode =
            peer(0).ecConnection()->makeMiscManager(Qn::kSystemAccess)->
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

std::vector<std::unique_ptr<PeerWrapper>> SystemMergeFixture::takeServers()
{
    return std::exchange(m_servers, {});
}

} // namespace test
} // namespace ec2
