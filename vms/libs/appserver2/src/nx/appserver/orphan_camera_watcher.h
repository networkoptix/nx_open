#pragma once

#include <QObject>
#include <QtCore/QTimer>

#include <common/common_module_aware.h>
#include <set>
#include <chrono>

#include <nx/utils/uuid.h>

class QnCommonModule;

namespace nx {
namespace appserver {

/** Monitor cameras without parent server and remove them.*/
class OrphanCameraWatcher : public QObject, public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QnCommonModuleAware;

public:
    OrphanCameraWatcher(QnCommonModule* commonModule);
    void start();
    void update();

    /** Just emits doChangeInterval that changes the interval in the timer's thread. */
    void changeIntervalAsync(std::chrono::milliseconds interval);

signals:
    void doStart();
    void doChangeInterval(std::chrono::milliseconds interval);

private:
    using Uuids=std::set<QnUuid>;
    Uuids m_previousOrphanCameras;
    QTimer m_timer;
    std::chrono::milliseconds m_updateInterval;
};

} // namespace appserver
} // namespace nx

Q_DECLARE_METATYPE(std::chrono::milliseconds);