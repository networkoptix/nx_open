#include "stree_manager.h"

#include <nx/utils/log/log.h>
#include <nx/fusion/model_functions.h>

namespace nx {
namespace cdb {

StreeManager::StreeManager(const QString& xmlFilePath) noexcept(false):
    m_impl(m_cdbAttrNameSet, xmlFilePath)
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

    return m_impl.search(realInput, output);
}

const nx::utils::stree::ResourceNameSet& StreeManager::resourceNameSet() const
{
    return m_impl.resourceNameSet();
}

} // namespace cdb
} // namespace nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cdb, StreeOperation,
    (nx::cdb::StreeOperation::authentication, "authentication")
    (nx::cdb::StreeOperation::authorization, "authorization")
    (nx::cdb::StreeOperation::getTemporaryCredentialsParameters, "getTemporaryCredentialsParameters")
)
