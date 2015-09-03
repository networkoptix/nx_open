/**********************************************************
* 7 may 2015
* a.kolesnikov
***********************************************************/

#ifndef CDB_ACCOUNT_DATA_H
#define CDB_ACCOUNT_DATA_H

#include <QtCore/QUrlQuery>

#include <stdint.h>
#include <string>

#include <plugins/videodecoder/stree/resourcecontainer.h>
#include <utils/common/model_functions_fwd.h>
#include <utils/common/uuid.h>
#include <utils/fusion/fusion_fwd.h>

#include <cdb/account_data.h>


namespace nx {
namespace cdb {

namespace api {
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(AccountStatus)
    QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((AccountStatus), (lexical))
}

namespace data {

class AccountData
:
    public api::AccountData,
    public stree::AbstractResourceReader
{
public:
    //!Implementation of \a stree::AbstractResourceReader::getAsVariant
    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
};

//TODO #ak add corresponding parser/serializer to fusion and remove this function
bool loadFromUrlQuery( const QUrlQuery& urlQuery, AccountData* const accountData );

#define AccountData_Fields (id)(login)(email)(passwordHa1)(fullName)(statusCode)


QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (AccountData),
    (json)(sql_record) )


}   //data
}   //cdb
}   //nx

#endif  //CDB_ACCOUNT_DATA_H
