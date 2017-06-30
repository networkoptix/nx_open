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
    const nx::utils::stree::ResourceNameSet& resourceNameSet,
    const QString& xmlFilePath) noexcept(false)
:
    nx::utils::stree::StreeManager(resourceNameSet, xmlFilePath)
{
}

void StreeManager::search(
    StreeOperation operation,
    const nx::utils::stree::AbstractResourceReader& input,
    nx::utils::stree::AbstractResourceWriter* const output) const
{
    nx::utils::stree::SingleResourceContainer operationRes(
        attr::operation, QnLexical::serialized(operation));
    nx::utils::stree::MultiSourceResourceReader realInput(operationRes, input);

    return nx::utils::stree::StreeManager::search(realInput, output);
}

}   //cdb
}   //nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cdb, StreeOperation,
    (nx::cdb::StreeOperation::authentication, "authentication")
    (nx::cdb::StreeOperation::authorization, "authorization")
    (nx::cdb::StreeOperation::getTemporaryCredentialsParameters, "getTemporaryCredentialsParameters")
)
