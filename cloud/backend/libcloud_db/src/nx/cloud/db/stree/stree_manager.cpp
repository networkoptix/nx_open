#include "stree_manager.h"

#include <nx/utils/log/log.h>
#include <nx/fusion/model_functions.h>

namespace nx::cloud::db {

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

} // namespace nx::cloud::db

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::db, StreeOperation,
    (nx::cloud::db::StreeOperation::authentication, "authentication")
    (nx::cloud::db::StreeOperation::authorization, "authorization")
    (nx::cloud::db::StreeOperation::getTemporaryCredentialsParameters, "getTemporaryCredentialsParameters")
)
