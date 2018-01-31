#pragma once

#include <memory>
#include <stdexcept>

#include "node.h"
#include "resourcecontainer.h"
#include "resourcenameset.h"

namespace nx {
namespace utils {
namespace stree {

class NX_UTILS_API StreeManager
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
        const nx::utils::stree::AbstractResourceReader& input,
        nx::utils::stree::AbstractResourceWriter* const output) const;
    const nx::utils::stree::ResourceNameSet& resourceNameSet() const;

    static std::unique_ptr<nx::utils::stree::AbstractNode> loadStree(
        QIODevice* const dataSource,
        const nx::utils::stree::ResourceNameSet& resourceNameSet);

private:
    std::unique_ptr<nx::utils::stree::AbstractNode> m_stree;
    const nx::utils::stree::ResourceNameSet& m_attrNameSet;
    const QString m_xmlFilePath;

    void loadStree() noexcept(false);
};

} // namespace stree
} // namespace utils
} // namespace nx
