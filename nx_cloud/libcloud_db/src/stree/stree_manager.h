/**********************************************************
* Sep 14, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_STREE_MANAGER_H
#define NX_CDB_STREE_MANAGER_H

#include <memory>
#include <stdexcept>

#include <plugins/videodecoder/stree/stree_manager.h>
#include <utils/common/model_functions_fwd.h>
#include <nx/utils/singleton.h>

#include "cdb_ns.h"
#include "settings.h"


namespace nx {
namespace cdb {

enum class StreeOperation
{
    authentication,
    authorization
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(StreeOperation)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((StreeOperation), (lexical))

class StreeManager
:
    public Singleton<StreeManager>,
    public stree::StreeManager
{
public:
    /*!
        Performs initial parsing
        \throw \a std::runtime_error in case of parse error
    */
    StreeManager(
        const stree::ResourceNameSet& resourceNameSet,
        const QString& xmlFilePath) throw(std::runtime_error);

    void search(
        StreeOperation,
        const stree::AbstractResourceReader& input,
        stree::AbstractResourceWriter* const output) const;
};

}   //cdb
}   //nx

#endif  //NX_CDB_STREE_MANAGER_H
