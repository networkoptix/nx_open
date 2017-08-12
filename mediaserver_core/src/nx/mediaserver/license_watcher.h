#pragma once

#include <QObject>
#include <QtCore/QTimer>

#include <common/common_module_aware.h>
#include <nx_ec/data/api_stored_file_data.h>
#include <nx/network/http/http_async_client.h>
#include <nx_ec/data/api_fwd.h>

class QnCommonModule;

namespace nx {
namespace mediaserver {

struct ServerLicenseInfo;

/**
 * Monitor unused layout's wallpapers in the database and remove it.
 */
class LicenseWatcher: public QObject, public QnCommonModuleAware
{
    Q_OBJECT;
    using base_type = QnCommonModuleAware;
public:
    LicenseWatcher(QnCommonModule* commonModule);
    void start();
public slots:
    void update();
private:
    void onHttpClientDone();
    ServerLicenseInfo licenseData() const;
private:
    QTimer m_timer;
    std::unique_ptr<nx_http::AsyncClient> m_httpClient;
};

} // namespace mediaserverm_previousData
} // namespace nx
