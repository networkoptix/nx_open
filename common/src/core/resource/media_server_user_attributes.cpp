/**********************************************************
* 6 oct 2014
* akolesnikov
***********************************************************/

#include "media_server_user_attributes.h"
#include <nx/fusion/model_functions.h>

QnMediaServerUserAttributes::QnMediaServerUserAttributes():
    serverId(),
    maxCameras(0),
    isRedundancyEnabled(false),
    name(),
    backupSchedule()
{
}

void QnMediaServerUserAttributes::assign(
    const QnMediaServerUserAttributes   &right,
    QSet<QByteArray>                    *const modifiedFields
)
{
    if (isRedundancyEnabled != right.isRedundancyEnabled)
        modifiedFields->insert("redundancyChanged");

    if (name != right.name)
        modifiedFields->insert("nameChanged");

    if (backupSchedule != right.backupSchedule)
        modifiedFields->insert("backupScheduleChanged");

    *this = right;
}



QnMediaServerUserAttributesPool::QnMediaServerUserAttributesPool(QObject *parent):
    QObject(parent)
{
    setElementInitializer( []( const QnUuid& serverId, QnMediaServerUserAttributesPtr& userAttributes ){
        userAttributes = QnMediaServerUserAttributesPtr( new QnMediaServerUserAttributes() );
        userAttributes->serverId = serverId;
    } );
}

QnMediaServerUserAttributesList QnMediaServerUserAttributesPool::getAttributesList(const QList<QnUuid>& idList) {
    QnMediaServerUserAttributesList valList;
    valList.reserve( idList.size() );
    for( const QnUuid id: idList )
        valList.push_back( get(id) );
    return valList;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnMediaServerUserAttributes), (eq), _Fields)
