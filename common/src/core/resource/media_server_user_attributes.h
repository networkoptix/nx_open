/**********************************************************
* 6 oct 2014
* akolesnikov
***********************************************************/

#ifndef MEDIA_SERVER_USER_ATTRIBUTES_H
#define MEDIA_SERVER_USER_ATTRIBUTES_H

#include <utils/common/singleton.h>

#include <utils/common/model_functions_fwd.h>
#include <core/resource/general_attribute_pool.h>
#include <core/resource/resource_fwd.h>

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
#define QnMediaServerUserAttributes_Fields (serverID)(maxCameras)(isRedundancyEnabled)(name)
QN_FUSION_DECLARE_FUNCTIONS(QnMediaServerUserAttributes, (eq))

Q_DECLARE_METATYPE(QnMediaServerUserAttributes)
Q_DECLARE_METATYPE(QnMediaServerUserAttributesPtr)
Q_DECLARE_METATYPE(QnMediaServerUserAttributesList)


class QnMediaServerUserAttributesPool
:
    public QObject,
    public QnGeneralAttributePool<QnUuid, QnMediaServerUserAttributesPtr>,
    public Singleton<QnMediaServerUserAttributesPool>
{
    Q_OBJECT
public:
    QnMediaServerUserAttributesPool();

    QnMediaServerUserAttributesList getAttributesList( const QList<QnUuid>& idList );
};

#endif  //MEDIA_SERVER_USER_ATTRIBUTES_H
