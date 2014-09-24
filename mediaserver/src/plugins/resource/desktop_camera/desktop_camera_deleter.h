#ifndef _DESKTOP_CAMERA_DELETER_H__
#define _DESKTOP_CAMERA_DELETER_H__

#ifdef ENABLE_DESKTOP_CAMERA

#include <core/resource/resource_fwd.h>

/** Utility class that deletes desktop cameras after some time of inactivity. */
class QnDesktopCameraDeleter: public QObject {
    Q_OBJECT
public:
    QnDesktopCameraDeleter(QObject *parent = NULL);
private:
    QnResourceList m_queuedToDelete;
};

#endif //ENABLE_DESKTOP_CAMERA

#endif // _DESKTOP_CAMERA_DELETER_H__
