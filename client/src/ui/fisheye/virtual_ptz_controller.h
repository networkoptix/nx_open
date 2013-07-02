#ifndef __VIRTUAL_PTZ_CONTROLLER_H__
#define __VIRTUAL_PTZ_CONTROLLER_H__

#include <QVector3D>

class QnVirtualPtzController
{
public:
    virtual ~QnVirtualPtzController() {}

    virtual void setMovement(const QVector3D& motion) = 0;
    virtual QVector3D movement() = 0;
};

#endif // __VIRTUAL_PTZ_CONTROLLER_H__
