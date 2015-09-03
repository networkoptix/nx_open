/**********************************************************
* 6 may 2015
* a.kolesnikov
***********************************************************/

#ifndef CLOUD_DB_SYSTEM_DATA_H
#define CLOUD_DB_SYSTEM_DATA_H

#include <QtCore/QUrlQuery>

#include <string>
#include <vector>

#include <plugins/videodecoder/stree/resourcecontainer.h>
#include <utils/common/model_functions_fwd.h>
#include <utils/common/uuid.h>
#include <utils/fusion/fusion_fwd.h>

#include <cdb/system_data.h>


namespace nx {
namespace cdb {

namespace api
{

namespace SystemAccessRole
{
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Value)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((Value), (lexical))
}

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SystemStatus)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((SystemStatus), (lexical))

}   //api

namespace data {

class SubscriptionData
:
    public api::SubscriptionData
{
public:
};

//!Information required to register system in cloud
class SystemRegistrationData
:
    public api::SystemRegistrationData,
    public stree::AbstractResourceReader
{
public:
    //!Implementation of \a stree::AbstractResourceReader::getAsVariant
    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
};

#define SystemRegistrationData_Fields (name)

class SystemRegistrationDataWithAccountID
:
    public SystemRegistrationData
{
public:
    QnUuid accountID;

    SystemRegistrationDataWithAccountID(SystemRegistrationData&& right)
    :
        SystemRegistrationData(std::move(right))
    {
    }
};

//TODO #ak add corresponding parser/serializer to fusion and remove this function
bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemRegistrationData* const systemData );


class SystemData
:
    public api::SystemData,
    public stree::AbstractResourceReader
{
public:
    //!Implementation of \a stree::AbstractResourceReader::getAsVariant
    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
};

//TODO #ak add corresponding parser/serializer to fusion and remove this function
//bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemData* const systemData );

#define SystemData_Fields (id)(name)(authKey)(ownerAccountID)(status)(cloudConnectionSubscriptionStatus)


class SystemDataList
{
public:
    std::vector<SystemData> systems;
};

#define SystemDataList_Fields (systems)


class SystemSharing
:
    public api::SystemSharing,
    public stree::AbstractResourceReader
{
public:
    //!Implementation of \a stree::AbstractResourceReader::getAsVariant
    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
};

bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemSharing* const systemSharing );

#define SystemSharing_Fields (accountID)(systemID)(accessRole)


//!for requests passing just system id
class SystemID
:
    public stree::AbstractResourceReader
{
public:
    QnUuid id;

    //!Implementation of \a stree::AbstractResourceReader::getAsVariant
    virtual bool getAsVariant(int resID, QVariant* const value) const override;
};

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemID* const systemID);

#define SystemID_Fields (id)


QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (SystemRegistrationData)(SystemData)(SystemSharing)(SystemID),
    (json)(sql_record) )

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (SystemDataList),
    (json))


}   //data
}   //cdb
}   //nx

#endif //CLOUD_DB_SYSTEM_DATA_H
