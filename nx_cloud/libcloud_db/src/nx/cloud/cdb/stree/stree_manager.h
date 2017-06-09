/**********************************************************
* Sep 14, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_STREE_MANAGER_H
#define NX_CDB_STREE_MANAGER_H

#include <memory>
#include <stdexcept>

#include <nx/utils/stree/stree_manager.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/singleton.h>

#include "cdb_ns.h"
#include "settings.h"


namespace nx {
namespace cdb {

enum class StreeOperation
{
    authentication,
    authorization,
    getTemporaryCredentialsParameters
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(StreeOperation)

class StreeManager
:
    public Singleton<StreeManager>,
    public nx::utils::stree::StreeManager
{
public:
    /*!
        Performs initial parsing
        \throw \a std::runtime_error in case of parse error
    */
    StreeManager(
        const nx::utils::stree::ResourceNameSet& resourceNameSet,
        const QString& xmlFilePath) noexcept(false);

    void search(
        StreeOperation,
        const nx::utils::stree::AbstractResourceReader& input,
        nx::utils::stree::AbstractResourceWriter* const output) const;
};

}   //cdb
}   //nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::cdb::StreeOperation), (lexical));

#endif  //NX_CDB_STREE_MANAGER_H
