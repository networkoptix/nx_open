#pragma once

#include <boost/optional.hpp>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <nx/network/cloud/cloud_module_url_fetcher.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/uuid.h>
#include <nx/utils/url.h>

#include <core/resource/resource_fwd.h>

class QnCommonModule;

namespace nx {
namespace vms {
namespace cloud_integration {

class AbstractEc2CloudConnector
{
public:
    virtual ~AbstractEc2CloudConnector() = default;

    virtual void startDataSynchronization(
        const std::string& peerId,
        const nx::utils::Url& cloudUrl) = 0;

    virtual void stopDataSynchronization() = 0;
};

/**
 * Monitor runtime flags RF_CloudSync. Only one server at once should has it.
 */
class QnConnectToCloudWatcher:
    public QObject
{
    Q_OBJECT

public:
    QnConnectToCloudWatcher(
        QnCommonModule* commonModule,
        AbstractEc2CloudConnector* ec2CloudConnector);
    virtual ~QnConnectToCloudWatcher();

    void setCloudDbUrl(const nx::utils::Url &cloudDbUrl);
    std::optional<nx::utils::Url> cloudDbUrl() const;

private slots:
    void at_updateConnection();
    void restartTimer();
    void addCloudPeer(const nx::utils::Url& url);

private:
    QnCommonModule* m_commonModule;
    AbstractEc2CloudConnector* m_ec2CloudConnector;
    nx::utils::Url m_cloudPeerCandidateUrl;
    nx::utils::Url m_cloudUrl;
    QTimer m_timer;
    std::unique_ptr<nx::network::cloud::CloudDbUrlFetcher> m_cdbEndPointFetcher;
    std::unique_ptr<nx::network::http::AsyncClient> m_fetchPeerIdRequest;
    std::optional<nx::utils::Url> m_cloudDbUrl;
    mutable QnMutex m_mutex;

    void fetchCloudPeerIdAsync(const nx::utils::Url& url);
    void processFetchPeerIdResponse();
    void connectToCloudPeer(
        const std::string& peerId,
        const nx::utils::Url& url);
};

} // namespace cloud_integration
} // namespace vms
} // namespace nx
