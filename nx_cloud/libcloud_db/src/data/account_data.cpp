/**********************************************************
* Aug 6, 2015
* a.kolesnikov
***********************************************************/

#include "account_data.h"

#include <utils/common/model_functions.h>
#include <utils/network/buffer.h>

#include "stree/cdb_ns.h"


namespace nx {
namespace cdb {
namespace data {

bool AccountData::getAsVariant( int resID, QVariant* const value ) const
{
    switch( resID )
    {
        case attr::accountID:
            *value = QVariant::fromValue(id);
            return true;
        case attr::accountEmail:
            *value = QString::fromStdString(email);
            return true;

        default:
            return false;
    }
}

bool AccountActivationCode::getAsVariant(int /*resID*/, QVariant* const /*value*/) const
{
    //TODO #ak
    return false;
}


bool AccountUpdateData::getAsVariant(int resID, QVariant* const value) const
{
    //TODO #ak
    return false;
}


AccountUpdateDataWithEmail::AccountUpdateDataWithEmail(AccountUpdateData&& rhs)
:
    AccountUpdateData(std::move(rhs))
{
}


//QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
//    (AccountUpdateDataWithEmail),
//    (sql_record),
//    _Fields)

}   //data
}   //cdb
}   //nx
