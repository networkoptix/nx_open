#pragma once

#include <memory>

#include <QtCore/QString>

#include <nx/utils/test_support/module_instance_launcher.h>

#include <api/mediaserver_client.h>

#include "appserver2_process.h"
#include "../transaction/transaction.h"

namespace ec2 {
namespace test {

class MediaServerClientEx;

class PeerWrapper
{
public:
    PeerWrapper(const QString& dataDir);

    PeerWrapper(PeerWrapper&&) = default;
    PeerWrapper& operator=(PeerWrapper&&) = default;

    bool startAndWaitUntilStarted();

    bool configureAsLocalSystem();

    QnRestResult::Error mergeTo(const PeerWrapper& remotePeer);

    ec2::ErrorCode getTransactionLog(ec2::ApiTransactionDataList* result) const;

    SocketAddress endpoint() const;

    static bool areAllPeersHaveSameTransactionLog(
        const std::vector<std::unique_ptr<PeerWrapper>>& peers);

private:
    QString m_dataDir;
    nx::utils::test::ModuleLauncher<Appserver2ProcessPublic> m_process;
    QString m_systemName;
    nx_http::Credentials m_ownerCredentials;

    std::unique_ptr<MediaServerClientEx> prepareMediaServerClient() const;

    QString buildAuthKey(
        const nx::String& url,
        const nx_http::Credentials& credentials,
        const nx::String& nonce);
};

} // namespace test
} // namespace ec2
