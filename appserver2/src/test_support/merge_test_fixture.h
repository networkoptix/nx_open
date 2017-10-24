#pragma once

#include <memory>
#include <vector>

#include "peer_wrapper.h"

namespace ec2 {
namespace test {

class SystemMergeFixture
{
public:
    SystemMergeFixture();
    virtual ~SystemMergeFixture();

    bool initializeSingleServerSystems(int count);

    void whenMergeSystems();

    void thenAllServersSynchronizedData();

    QnRestResult::Error prevMergeResult() const;

private:
    QString m_tmpDir;
    std::vector<std::unique_ptr<PeerWrapper>> m_servers;
    QnRestResult::Error m_prevResult = QnRestResult::Error::NoError;
};

} // namespace test
} // namespace ec2
