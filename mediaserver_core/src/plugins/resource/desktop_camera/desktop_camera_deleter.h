#pragma once

#ifdef ENABLE_DESKTOP_CAMERA

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>

/** Utility class that deletes desktop cameras after some time of inactivity. */
class QnDesktopCameraDeleter: public QObject, public QnCommonModuleAware
{
    Q_OBJECT

public:
    QnDesktopCameraDeleter(QObject *parent);

private:
    /** Remove desktop cameras that have been offline for more than one iteration. */
    void deleteQueuedResources();

    /** Queue to removing all resources that went offline since the last check. */
    void updateQueue();

private:
    QnResourceList m_queuedToDelete;
};

#endif //ENABLE_DESKTOP_CAMERA
