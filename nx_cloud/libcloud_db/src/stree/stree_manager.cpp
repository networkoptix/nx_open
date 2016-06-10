/**********************************************************
* Sep 14, 2015
* a.kolesnikov
***********************************************************/

#include "stree_manager.h"

#include <nx/utils/log/log.h>
#include <nx/fusion/model_functions.h>


namespace nx {
namespace cdb {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(StreeOperation,
    (StreeOperation::authentication, "authentication")
    (StreeOperation::authorization, "authorization")
)


StreeManager::StreeManager(
    const stree::ResourceNameSet& resourceNameSet,
    const QString& xmlFilePath) throw(std::runtime_error)
:
    stree::StreeManager(resourceNameSet, xmlFilePath)
{
}

void StreeManager::search(
    StreeOperation operation,
    const stree::AbstractResourceReader& input,
    stree::AbstractResourceWriter* const output) const
{
    stree::SingleResourceContainer operationRes(
        attr::operation, QnLexical::serialized(operation));
    stree::MultiSourceResourceReader realInput(operationRes, input);

    return stree::StreeManager::search(realInput, output);
}

}   //cdb
}   //nx
