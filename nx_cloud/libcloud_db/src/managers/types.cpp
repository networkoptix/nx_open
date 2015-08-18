/**********************************************************
* Aug 18, 2015
* a.kolesnikov
***********************************************************/

#include "types.h"


namespace nx {
namespace cdb {


ResultCode fromDbResultCode( nx::db::DBResult dbResult )
{
    switch( dbResult )
    {
        case nx::db::DBResult::ok:
            return ResultCode::ok;

        case nx::db::DBResult::notFound:
            return ResultCode::notFound;

        case nx::db::DBResult::ioError:
        default:
            return ResultCode::dbError;
    }
}

}   //cdb
}   //nx
