#pragma once

#include <memory>
#include <string>

#include <QtCore/QString>

#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
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

    void addSetting(const std::string& name, const std::string& value);

    bool startAndWaitUntilStarted();

    bool configureAsLocalSystem();

    bool saveCloudSystemCredentials(
        const std::string& systemId,
        const std::string& authKey,
        const std::string& ownerAccountEmail);

    QnRestResult::Error mergeTo(const PeerWrapper& remotePeer);

    ec2::ErrorCode getTransactionLog(ec2::ApiTransactionDataList* result) const;

    SocketAddress endpoint() const;

    ec2::AbstractECConnection* ecConnection();

    QnUuid id() const;

    nx::hpm::api::SystemCredentials getCloudCredentials() const;

    std::unique_ptr<MediaServerClient> mediaServerClient() const;

    nx::utils::test::ModuleLauncher<Appserver2ProcessPublic>& process();

    static bool areAllPeersHaveSameTransactionLog(
        const std::vector<std::unique_ptr<PeerWrapper>>& peers);

    static bool arePeersInterconnected(
        const std::vector<std::unique_ptr<PeerWrapper>>& peers);

private:
    QString m_dataDir;
    nx::utils::test::ModuleLauncher<Appserver2ProcessPublic> m_process;
    QString m_systemName;
    nx_http::Credentials m_ownerCredentials;
    nx::hpm::api::SystemCredentials m_cloudCredentials;

    std::unique_ptr<MediaServerClientEx> prepareMediaServerClient() const;
};

} // namespace test
} // namespace ec2
