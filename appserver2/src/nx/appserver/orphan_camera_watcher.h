#pragma once

#include <QObject>
#include <QtCore/QTimer>

#include <common/common_module_aware.h>
#include <nx_ec/data/api_stored_file_data.h>

#include <set>
#include <chrono>

class QnCommonModule;

namespace nx {
namespace appserver {

/**
 * Monitor cameras without parent server and remove them.
 */
class OrphanCameraWatcher : public QObject, public QnCommonModuleAware
{
    enum class UpdateIntervalType { manualInterval, shortInterval1, shortInterval2, longInterval };
    /*
     * Update interval type graph:
     *
     *              ------> manualInterval <----------        ------
     *             |                ^                 |      |      |
     *             |                |                 |      V      |
     * ---> shortInterval1 ---> shortInterval2 ---> longInterval ---
     */

    Q_OBJECT
    using base_type = QnCommonModuleAware;

public:
    OrphanCameraWatcher(QnCommonModule* commonModule);
    void start();
    void update();

    /** changeIntervalAsync just emits doChangeInterval, that restarts timer in timer's thread */
    void changeIntervalAsync(std::chrono::milliseconds interval);

signals:
    void doStart();
    void doChangeInterval(std::chrono::milliseconds interval);

private:
    using Uuids=std::set<QnUuid>;
    Uuids m_previousOrphanCameras;
    QTimer m_timer;
    std::chrono::milliseconds m_updateInterval;
    UpdateIntervalType m_updateIntervalType;
};

} // namespace appserver
} // namespace nx

Q_DECLARE_METATYPE(std::chrono::milliseconds);