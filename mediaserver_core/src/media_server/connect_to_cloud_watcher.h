#pragma once

#include <QObject>
#include <QElapsedTimer>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/network/cloud/cdb_endpoint_fetcher.h>
#include <nx/utils/timer_manager.h>

/**
 * Monitor runtime flags RF_CloudSync. Only one server at once should has it
 */

class QnConnectToCloudWatcher: public QObject
{
    Q_OBJECT
public:
    QnConnectToCloudWatcher();
    virtual ~QnConnectToCloudWatcher();
private slots:
    void at_updateConnection();
private:
    QUrl m_cloudUrl;
    nx::utils::TimerManager::TimerGuard m_timerID;
    std::unique_ptr<nx::network::cloud::CloudModuleEndPointFetcher> m_cdbEndPointFetcher;
};
