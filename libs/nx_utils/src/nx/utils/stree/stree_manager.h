// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include <nx/utils/buffer.h>

#include "node.h"
#include "resourcecontainer.h"
#include "resourcenameset.h"

namespace nx::utils::stree {

enum ParseFlag
{
    ignoreUnknownResources = 0x01,
};

class NX_UTILS_API StreeManager
{
public:
    /**
     * Performs initial parsing.
     * @throw std::runtime_error in case of parse error.
     */
    StreeManager(
        const nx::utils::stree::ResourceNameSet& resourceNameSet,
        const std::string& xmlFilePath) noexcept(false);

    void search(
        const nx::utils::stree::AbstractResourceReader& input,
        nx::utils::stree::AbstractResourceWriter* const output) const;

    const nx::utils::stree::ResourceNameSet& resourceNameSet() const;

    static std::unique_ptr<nx::utils::stree::AbstractNode> loadStree(
        const nx::Buffer& data,
        const nx::utils::stree::ResourceNameSet& resourceNameSet,
        int parseFlags = 0);

private:
    std::unique_ptr<nx::utils::stree::AbstractNode> m_stree;
    const nx::utils::stree::ResourceNameSet& m_attrNameSet;
    const std::string m_xmlFilePath;

    void loadStree() noexcept(false);
};

} // namespace nx::utils::stree
