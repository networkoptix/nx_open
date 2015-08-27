/**********************************************************
* 6 may 2015
* a.kolesnikov
***********************************************************/

#ifndef CLOUD_DB_SYSTEM_DATA_H
#define CLOUD_DB_SYSTEM_DATA_H

#include <QtCore/QUrlQuery>

#include <string>

#include <plugins/videodecoder/stree/resourcecontainer.h>
#include <utils/common/model_functions_fwd.h>
#include <utils/common/uuid.h>
#include <utils/fusion/fusion_fwd.h>


namespace nx {
namespace cdb {
namespace data {


class SubscriptionData
{
public:
    std::string productID;
    std::string systemID;
};

//!Information required to register system in cloud
class SystemRegistrationData
:
    public stree::AbstractResourceReader
{
public:
    //!Not unique system name
    std::string name;

    //!Implementation of \a stree::AbstractResourceReader::getAsVariant
    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
};

class SystemRegistrationDataWithAccountID
:
    public SystemRegistrationData
{
public:
    QnUuid accountID;

    SystemRegistrationDataWithAccountID( SystemRegistrationData&& right )
    :
        SystemRegistrationData( std::move( right ) )
    {
    }
};

//TODO #ak add corresponding parser/serializer to fusion and remove this function
bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemRegistrationData* const systemData );

#define SystemRegistrationData_Fields (name)


enum SystemStatus
{
    ssInvalid = 0,
    //!System has been bound but not a single request from that system has been received by cloud
    ssNotActivated = 1,
    ssActivated = 2
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION( SystemStatus )
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES( (SystemStatus), (lexical) )

class SystemData
:
    public stree::AbstractResourceReader
{
public:
    //!Globally unique id assigned to the system. Usually, it is GUID
    std::string id;
    //!Not unique system name
    std::string name;
    //!Key, system uses to authenticate requests to any cloud module
    std::string authKey;
    std::string ownerAccountID;
    SystemStatus status;
    //!a true, if cloud connection is activated for this system
    bool cloudConnectionSubscriptionStatus;

    SystemData()
    :
        status( ssInvalid ),
        cloudConnectionSubscriptionStatus( false )
    {
    }

    //!Implementation of \a stree::AbstractResourceReader::getAsVariant
    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
};

//TODO #ak add corresponding parser/serializer to fusion and remove this function
//bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemData* const systemData );

#define SystemData_Fields (id)(name)(authKey)(ownerAccountID)(status)(cloudConnectionSubscriptionStatus)


QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (SystemRegistrationData)(SystemData),
    (json)(sql_record) )


}   //data
}   //cdb
}   //nx

#endif //CLOUD_DB_SYSTEM_DATA_H
