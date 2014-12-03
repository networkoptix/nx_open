/**********************************************************
* 6 oct 2014
* akolesnikov
***********************************************************/

#include "media_server_user_attributes.h"


QnMediaServerUserAttributes::QnMediaServerUserAttributes()
:
    maxCameras(0),
    isRedundancyEnabled(false)
{
}

void QnMediaServerUserAttributes::assign( const QnMediaServerUserAttributes& right, QSet<QByteArray>* const modifiedFields )
{
    if (isRedundancyEnabled != right.isRedundancyEnabled)
        modifiedFields->insert("redundancyChanged");

    if (name != right.name)
        modifiedFields->insert("nameChanged");

    *this = right;
}



QnMediaServerUserAttributesPool::QnMediaServerUserAttributesPool()
{
    setElementInitializer( []( const QnUuid& serverID, QnMediaServerUserAttributesPtr& userAttributes ){
        userAttributes = QnMediaServerUserAttributesPtr( new QnMediaServerUserAttributes() );
        userAttributes->serverID = serverID;
    } );
}
