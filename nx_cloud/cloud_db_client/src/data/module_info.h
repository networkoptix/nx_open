/**********************************************************
* Sep 10, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_MODULE_INFO_H
#define NX_CDB_CL_MODULE_INFO_H

#include <utils/common/model_functions_fwd.h>
#include <utils/fusion/fusion_fwd.h>

#include <cdb/module_info.h>


namespace nx {
namespace cdb {
namespace api {

//TODO #ak add corresponding parser/serializer to fusion and remove this function
//bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountData* const accountData);
//void serializeToUrlQuery(const AccountData&, QUrlQuery* const urlQuery);

#define ModuleInfo_Fields (realm)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (ModuleInfo),
    (json) )

}   //api
}   //cdb
}   //nx

#endif  //NX_CDB_CL_MODULE_INFO_H
