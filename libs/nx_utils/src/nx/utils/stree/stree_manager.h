// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include <nx/utils/buffer.h>

#include "node.h"
#include "attribute_dictionary.h"

namespace nx::utils::stree {

class NX_UTILS_API StreeManager
{
public:
    /**
     * Performs initial parsing.
     * @throw std::runtime_error in case of parse error.
     */
    StreeManager(const std::string& xmlFilePath) noexcept(false);

    void search(
        const nx::utils::stree::AbstractAttributeReader& input,
        nx::utils::stree::AbstractAttributeWriter* const output) const;

    /**
     * Parse tree from the provided xml.
     */
    static std::unique_ptr<nx::utils::stree::AbstractNode> loadStree(const nx::Buffer& xmlData);

private:
    std::unique_ptr<nx::utils::stree::AbstractNode> m_stree;
    const std::string m_xmlFilePath;

    void loadStree() noexcept(false);
};

} // namespace nx::utils::stree
