#ifndef QN_PTZ_FWD_H
#define QN_PTZ_FWD_H

#include <QtCore/QSharedPointer>

template<class T>
class QnSpaceMapper;

template<class T>
class QnSpaceMapperPtr;

typedef QnSpaceMapperPtr<QVector3D> QnPtzMapperPtr;

class QnAbstractPtzController;
typedef QSharedPointer<QnAbstractPtzController> QnPtzControllerPtr;

#endif // QN_PTZ_FWD_H

