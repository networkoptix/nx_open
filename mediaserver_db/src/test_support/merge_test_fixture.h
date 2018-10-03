#pragma once

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "peer_wrapper.h"

namespace ec2 {
namespace test {

class SystemMergeFixture
{
public:
    SystemMergeFixture(const std::string& baseDirectory);
    virtual ~SystemMergeFixture() = default;

    void addSetting(const std::string& name, const std::string& value);

    bool initializeSingleServerSystems(int count);

    int peerCount() const;

    const PeerWrapper& peer(int index) const;
    PeerWrapper& peer(int index);

    QnRestResult::Error mergePeers(
        PeerWrapper& what,
        PeerWrapper& to,
        std::vector<MergeAttributes::Attribute> attributes);

    QnRestResult::Error mergeSystems();

    void waitUntilAllServersAreInterconnected();
    void waitUntilAllServersSynchronizedData();

    nx::vms::api::SystemMergeHistoryRecordList waitUntilMergeHistoryIsAdded();

    QnRestResult::Error prevMergeResult() const;

    std::vector<std::unique_ptr<PeerWrapper>> takeServers();

private:
    const std::string m_baseDirectory;
    std::vector<std::unique_ptr<PeerWrapper>> m_servers;
    QnRestResult::Error m_prevResult = QnRestResult::Error::NoError;
    std::map<std::string, std::string> m_settings;
};

} // namespace test
} // namespace ec2
