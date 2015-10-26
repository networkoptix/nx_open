/**********************************************************
* 6 oct 2014
* akolesnikov
***********************************************************/

#include "media_server_user_attributes.h"
#include <utils/common/model_functions.h>

QnMediaServerUserAttributes::QnMediaServerUserAttributes()
:
    maxCameras(0),
    isRedundancyEnabled(false),
    backupType(Qn::Backup_Disabled),
    backupDaysOfTheWeek(0x7f), // all days of week
    backupStart(0), // midnight
    backupDuration(-1), // unlimited duration
    backupBitrate(-1) // unlimited
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

    if (backupType != right.backupType)
        modifiedFields->insert("backupType");

    if (backupDaysOfTheWeek != right.backupDaysOfTheWeek)
        modifiedFields->insert("backupDaysOfTheWeekChanged");

    if (backupBitrate != right.backupBitrate)
        modifiedFields->insert("backupBitrateChanged");

    if (backupDuration != right.backupDuration)
        modifiedFields->insert("backupDurationChanged");

    if (backupStart != right.backupStart)
        modifiedFields->insert("backupStartChanged");

    *this = right;
}



QnMediaServerUserAttributesPool::QnMediaServerUserAttributesPool()
{
    setElementInitializer( []( const QnUuid& serverID, QnMediaServerUserAttributesPtr& userAttributes ){
        userAttributes = QnMediaServerUserAttributesPtr( new QnMediaServerUserAttributes() );
        userAttributes->serverID = serverID;
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
