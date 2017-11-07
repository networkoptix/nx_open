#pragma once

#include <boost/optional.hpp>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <nx/network/cloud/cloud_module_url_fetcher.h>
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

    virtual void startDataSynchronization(const nx::utils::Url& cloudUrl) = 0;
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

private slots:
    void at_updateConnection();
    void restartTimer();
    void addCloudPeer(nx::utils::Url url);

private:
    QnCommonModule* m_commonModule;
    AbstractEc2CloudConnector* m_ec2CloudConnector;
    nx::utils::Url m_cloudUrl;
    QTimer m_timer;
    std::unique_ptr<nx::network::cloud::CloudModuleUrlFetcher> m_cdbEndPointFetcher;
    boost::optional<nx::utils::Url> m_cloudDbUrl;
};

} // namespace cloud_integration
} // namespace vms
} // namespace nx
