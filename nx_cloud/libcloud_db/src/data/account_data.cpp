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

        default:
            return false;
    }
}

}   //data
}   //cdb
}   //nx
