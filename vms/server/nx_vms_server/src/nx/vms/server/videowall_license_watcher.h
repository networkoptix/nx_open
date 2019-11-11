#pragma once

#include <QObject>
#include <QtCore/QTimer>

#include <nx/vms/server/server_module_aware.h>
#include <nx/network/http/http_async_client.h>
#include <nx_ec/data/api_fwd.h>

namespace nx {
namespace vms::server {

/**
 * Periodically check validity of VideoWall licenses and update runtime info
 * with the timestamp when VideoWall licenses became invalid/
 */
class VideoWallLicenseWatcher: public QObject, public /*mixin*/ ServerModuleAware
{
    Q_OBJECT
    using base_type = ServerModuleAware;

public:
    VideoWallLicenseWatcher(QnMediaServerModule* serverModule);
    virtual ~VideoWallLicenseWatcher();

    void start();
    void stop();

private:
    void updateRuntimeInfoAfterVideoWallLicenseOverflowTransaction(
        qint64 prematureVideoWallLicenseExperationDate);

private slots:
    void at_checkLicenses();

private:
    QTimer m_licenseTimer;
    int m_tooManyVideoWallsCounter = 0;
    std::chrono::milliseconds m_videoWallStopTime;
};

} // namespace mediaserverm_previousData
} // namespace nx
