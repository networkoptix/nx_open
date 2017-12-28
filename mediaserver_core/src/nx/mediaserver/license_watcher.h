#pragma once

#include <QObject>
#include <QtCore/QTimer>

#include <common/common_module_aware.h>
#include <nx/network/http/http_async_client.h>
#include <nx_ec/data/api_fwd.h>

namespace nx {
namespace mediaserver {

struct ServerLicenseInfo;

/**
 * Periodically check on license server if license was removed or updated.
 */
class LicenseWatcher: public QObject, public QnCommonModuleAware
{
    Q_OBJECT;
    using base_type = QnCommonModuleAware;

public:
    LicenseWatcher(QnCommonModule* commonModule);
    virtual ~LicenseWatcher();
    void start();

private:
    void startUpdate();
    void processResponse(QByteArray responseData);
    void stopHttpClient();
    ServerLicenseInfo licenseData() const;

private:
    QTimer m_timer;
    QnMutex m_mutex;
    std::unique_ptr<nx::network::http::AsyncClient> m_httpClient;
};

} // namespace mediaserverm_previousData
} // namespace nx
