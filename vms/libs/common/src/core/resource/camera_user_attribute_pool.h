#pragma once

#include <core/resource/general_attribute_pool.h>

#include <nx/utils/singleton.h>

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

class QnCameraUserAttributePool:
    public QObject,
    public QnGeneralAttributePool<QnUuid, QnCameraUserAttributesPtr>
{
    Q_OBJECT
public:
    explicit QnCameraUserAttributePool(QObject* parent = nullptr);
    virtual ~QnCameraUserAttributePool();

    QnCameraUserAttributesList getAttributesList( const QList<QnUuid>& idList );
};
