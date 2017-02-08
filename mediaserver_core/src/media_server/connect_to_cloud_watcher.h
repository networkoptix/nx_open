#pragma once

#include <QObject>
#include <QElapsedTimer>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/network/cloud/cloud_module_url_fetcher.h>

namespace ec2 {
    class QnTransactionMessageBus;
}

/**
 * Monitor runtime flags RF_CloudSync. Only one server at once should has it
 */

class QnConnectToCloudWatcher: public QObject
{
    Q_OBJECT
public:
    QnConnectToCloudWatcher(ec2::QnTransactionMessageBus* messageBus);
    virtual ~QnConnectToCloudWatcher();
private slots:
    void at_updateConnection();
    void restartTimer();
private:
    QUrl m_cloudUrl;
    QTimer m_timer;
    std::unique_ptr<nx::network::cloud::CloudModuleUrlFetcher> m_cdbEndPointFetcher;
    ec2::QnTransactionMessageBus* m_messageBus;
};
