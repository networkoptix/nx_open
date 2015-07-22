/**********************************************************
* 1 oct 2014
* akolesnikov
***********************************************************/

#ifndef CAMERA_USER_ATTRIBUTE_POOL_H
#define CAMERA_USER_ATTRIBUTE_POOL_H

#include <core/resource/general_attribute_pool.h>

#include <utils/common/singleton.h>

#include "camera_user_attributes.h"


//!Pool of \a QnCameraUserAttributes
/*!
    Example of adding element to pool and setting some values:
    \code {*.cpp}
    QnCameraUserAttributePool::ScopedLock lk( QnCameraUserAttributePool::instance(), resourceID );
    if( !lk->isAudioEnabled() )
        lk->setAudioEnabled( true );
    lk->setCameraControlDisabled( true );
    \endcode

    \a QnCameraUserAttributePool::ScopedLock constructor will block if element with id \a resourceID is already used
*/

class QnCameraUserAttributePool
:
    public QObject,
    public QnGeneralAttributePool<QnUuid, QnCameraUserAttributesPtr>,
    public Singleton<QnCameraUserAttributePool>
{
    Q_OBJECT
public:
    QnCameraUserAttributePool();
    virtual ~QnCameraUserAttributePool();

    QnCameraUserAttributesList getAttributesList( const QList<QnUuid>& idList );
};

#endif  //CAMERA_USER_ATTRIBUTE_POOL_H
