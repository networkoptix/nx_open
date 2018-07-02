/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#pragma once

#include <memory>

#include <nx/utils/url.h>
#include "base_ec2_connection.h"
#include "ec2_statictics_reporter.h"
#include "nx/appserver/orphan_camera_watcher.h"

#include "server_query_processor.h"

#include <nx/vms/api/data/timestamp.h>

namespace ec2 {

class LocalConnectionFactory;

// TODO: #2.4 remove Ec2 prefix to avoid ec2::Ec2DirectConnection
class Ec2DirectConnection: public BaseEc2Connection<ServerQueryProcessorAccess>
{
    using base_type = BaseEc2Connection<ServerQueryProcessorAccess>;

public:
    Ec2DirectConnection(
        const LocalConnectionFactory* connectionFactory,
        ServerQueryProcessorAccess* queryProcessor,
        const QnConnectionInfo& connectionInfo,
        const nx::utils::Url& dbUrl);
    virtual ~Ec2DirectConnection();

    //!Implementation of ec2::AbstractECConnection::connectionInfo
    virtual QnConnectionInfo connectionInfo() const override;
    virtual void updateConnectionUrl(const nx::utils::Url& url) override;

    bool initialized() const;

    Ec2StaticticsReporter* getStaticticsReporter();
    nx::appserver::OrphanCameraWatcher* orphanCameraWatcher();

    virtual nx::vms::api::Timestamp getTransactionLogTime() const override;
    virtual void setTransactionLogTime(nx::vms::api::Timestamp value) override;

    ec2::detail::QnDbManager* getDb() const;
    virtual void startReceivingNotifications() override;

private:
    const QnConnectionInfo m_connectionInfo;
    bool m_isInitialized;
    std::unique_ptr<Ec2StaticticsReporter> m_staticticsReporter;
    std::unique_ptr<nx::appserver::OrphanCameraWatcher> m_orphanCameraWatcher;
};
typedef std::shared_ptr<Ec2DirectConnection> Ec2DirectConnectionPtr;
typedef std::shared_ptr<nx::appserver::OrphanCameraWatcher> OrphanCameraWatcherPtr;

} // namespace ec2
