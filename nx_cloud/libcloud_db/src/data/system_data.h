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

#include <cloud_db_client/src/data/system_data.h>


namespace nx {
namespace cdb {
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

class SystemData
:
    public api::SystemData,
    public stree::AbstractResourceReader
{
public:
    //!Implementation of \a stree::AbstractResourceReader::getAsVariant
    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
};


class SystemDataList
:
    public api::SystemDataList
{
public:
};

class SystemSharing
:
    public api::SystemSharing,
    public stree::AbstractResourceReader
{
public:
    //!Implementation of \a stree::AbstractResourceReader::getAsVariant
    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
};

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
    (SystemID),
    (json)(sql_record) )

}   //data
}   //cdb
}   //nx

#endif //CLOUD_DB_SYSTEM_DATA_H
