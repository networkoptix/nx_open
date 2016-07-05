/**********************************************************
* 30 apr 2014
* a.kolesnikov
***********************************************************/

#include "rdir_synchronization_operation.h"


namespace detail
{
    namespace ResultCode
    {
        QString toString( Value val )
        {
            switch( val )
            {
                case success:
                    return "success";
                case downloadFailure:
                    return "downloadFailure";
                case writeFailure:
                    return "writeFailure";
                case interrupted:
                    return "interrupted";
                case unknownError:
                default:
                    return "unknownError";
            }
        }
    }
}
