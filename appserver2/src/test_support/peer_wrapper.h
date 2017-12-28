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

// TODO: #ak Get rid of this class. ec2GetTransactionLog should be in MediaServerClient.
// Though, it requires some refactoring: moving ec2::ApiTransactionDataList out of appserver2.
class MediaServerClientEx:
    public MediaServerClient
{
    using base_type = MediaServerClient;

public:
    MediaServerClientEx(const nx::utils::Url& baseRequestUrl);

    void ec2GetTransactionLog(
        const ApiTranLogFilter& filter,
        std::function<void(ec2::ErrorCode, ec2::ApiTransactionDataList)> completionHandler);

    ec2::ErrorCode ec2GetTransactionLog(
        const ApiTranLogFilter& filter,
        ec2::ApiTransactionDataList* result);
};

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

    nx::network::SocketAddress endpoint() const;

    ec2::AbstractECConnection* ecConnection();

    QnUuid id() const;

    nx::hpm::api::SystemCredentials getCloudCredentials() const;

    std::unique_ptr<MediaServerClientEx> mediaServerClient() const;

    nx::utils::test::ModuleLauncher<Appserver2ProcessPublic>& process();

    static bool areAllPeersHaveSameTransactionLog(
        const std::vector<std::unique_ptr<PeerWrapper>>& peers);

    static bool arePeersInterconnected(
        const std::vector<std::unique_ptr<PeerWrapper>>& peers);

private:
    QString m_dataDir;
    nx::utils::test::ModuleLauncher<Appserver2ProcessPublic> m_process;
    QString m_systemName;
    nx::network::http::Credentials m_ownerCredentials;
    nx::hpm::api::SystemCredentials m_cloudCredentials;

    std::unique_ptr<MediaServerClientEx> prepareMediaServerClient() const;
};

} // namespace test
} // namespace ec2
