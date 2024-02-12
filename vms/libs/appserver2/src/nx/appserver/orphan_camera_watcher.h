// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <set>

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <nx/utils/uuid.h>
#include <nx/vms/common/system_context_aware.h>

namespace nx {
namespace appserver {

/** Monitor cameras without parent server and remove them.*/
class OrphanCameraWatcher: public QObject, public nx::vms::common::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    OrphanCameraWatcher(nx::vms::common::SystemContext* systemContext);
    virtual ~OrphanCameraWatcher() override;
    void start();
    void update();
    void stop();

    /** Just emits doChangeInterval that changes the interval in the timer's thread. */
    void changeIntervalAsync(std::chrono::milliseconds interval);

signals:
    void doStart();
    void doChangeInterval(std::chrono::milliseconds interval);

private:
    using Uuids=std::set<nx::Uuid>;
    Uuids m_previousOrphanCameras;
    QTimer m_timer;
    std::chrono::milliseconds m_updateInterval;
};

} // namespace appserver
} // namespace nx
