/**********************************************************
* Sep 14, 2015
* a.kolesnikov
***********************************************************/

#include "stree_manager.h"

#include <nx/utils/log/log.h>
#include <nx/fusion/model_functions.h>


namespace nx {
namespace cdb {

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

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cdb, StreeOperation,
    (nx::cdb::StreeOperation::authentication, "authentication")
    (nx::cdb::StreeOperation::authorization, "authorization")
    (nx::cdb::StreeOperation::getTemporaryCredentialsParameters, "getTemporaryCredentialsParameters")
)
