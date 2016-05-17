/**********************************************************
* May 17, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <memory>
#include <stdexcept>

#include "node.h"
#include "resourcecontainer.h"
#include "resourcenameset.h"


namespace stree {

class StreeManager
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
        const stree::AbstractResourceReader& input,
        stree::AbstractResourceWriter* const output) const;
    const stree::ResourceNameSet& resourceNameSet() const;

private:
    std::unique_ptr<stree::AbstractNode> m_stree;
    const stree::ResourceNameSet& m_attrNameSet;
    const QString m_xmlFilePath;

    void loadStree() throw(std::runtime_error);
};

}   //namespace stree
