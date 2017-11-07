#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "peer_wrapper.h"

namespace ec2 {
namespace test {

class SystemMergeFixture
{
public:
    SystemMergeFixture();
    virtual ~SystemMergeFixture();

    void addSetting(const std::string& name, const std::string& value);

    bool initializeSingleServerSystems(int count);

    int peerCount() const;

    const PeerWrapper& peer(int index) const;
    PeerWrapper& peer(int index);

    void whenMergeSystems();

    void thenAllServersAreInterconnected();
    void thenAllServersSynchronizedData();

    QnRestResult::Error prevMergeResult() const;

private:
    QString m_tmpDir;
    std::vector<std::unique_ptr<PeerWrapper>> m_servers;
    QnRestResult::Error m_prevResult = QnRestResult::Error::NoError;
    std::map<std::string, std::string> m_settings;
};

} // namespace test
} // namespace ec2
