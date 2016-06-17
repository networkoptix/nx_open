/**********************************************************
* Dec 8, 2015
* a.kolesnikov
***********************************************************/

#include "notification.h"

#include <nx/fusion/model_functions.h>


namespace nx {
namespace cdb {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ActivateAccountData)(ActivateAccountNotification),
    (json),
    _Fields)

}   //cdb
}   //nx
