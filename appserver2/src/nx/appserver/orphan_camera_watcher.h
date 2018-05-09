#pragma once

#include <QObject>
#include <QtCore/QTimer>

#include <common/common_module_aware.h>
#include <set>
#include <nx/utils/uuid.h>

class QnCommonModule;

namespace nx {
namespace appserver {

/**
 * Monitor cameras without parent server and remove them.
 */
class OrphanCameraWatcher : public QObject, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QnCommonModuleAware;

public:
    OrphanCameraWatcher(QnCommonModule* commonModule);
    void start();
    void update();

    /* 
     changeIntervalAsync just emits doChangeInterval, 
     that restarts timer in timer's thread
    */
    void changeIntervalAsync(int ms);

signals:
    void doChangeInterval(int ms);

private:
    using Uuids=std::set<QnUuid>;
    Uuids m_previousOrphanCameras;
    QTimer m_timer;
    int m_updateIntervalMs;
};

} // namespace appserver
} // namespace nx
