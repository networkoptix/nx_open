/**********************************************************
* 6 oct 2014
* akolesnikov
***********************************************************/

#ifndef MEDIA_SERVER_USER_ATTRIBUTES_H
#define MEDIA_SERVER_USER_ATTRIBUTES_H

#include <QtCore/QSet>
#include <QtCore/QByteArray>

#include <utils/common/singleton.h>

#include "camera_user_attribute_pool.h"


class QnMediaServerUserAttributes
{
public:
    QnUuid serverID;
    int maxCameras;
    bool isRedundancyEnabled;
    QString name;

    QnMediaServerUserAttributes();
    void assign( const QnMediaServerUserAttributes& right, QSet<QByteArray>* const modifiedFields );
};

Q_DECLARE_METATYPE(QnMediaServerUserAttributes)
Q_DECLARE_METATYPE(QnMediaServerUserAttributesPtr)
Q_DECLARE_METATYPE(QnMediaServerUserAttributesList)


class QnMediaServerUserAttributesPool
:
    public QnGeneralAttributePool<QnUuid, QnMediaServerUserAttributesPtr>,
    public Singleton<QnMediaServerUserAttributesPool>
{
public:
    QnMediaServerUserAttributesPool();
};

#endif  //MEDIA_SERVER_USER_ATTRIBUTES_H
