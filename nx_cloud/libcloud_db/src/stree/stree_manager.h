/**********************************************************
* Sep 14, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_STREE_MANAGER_H
#define NX_CDB_STREE_MANAGER_H

#include <memory>
#include <stdexcept>

#include <plugins/videodecoder/stree/node.h>
#include <plugins/videodecoder/stree/resourcecontainer.h>
#include <plugins/videodecoder/stree/resourcenameset.h>
#include <utils/common/model_functions_fwd.h>


namespace nx {
namespace cdb {


enum class StreeOperation
{
    authentication
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(StreeOperation)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((StreeOperation), (lexical))

class StreeManager
{
public:
    /*!
        Performs initial parsing
        \throw \a std::runtime_error in case of parse error
    */
    StreeManager() throw(std::runtime_error);

    void search(
        StreeOperation,
        const stree::AbstractResourceReader& input,
        stree::AbstractResourceWriter* const output) const;

private:
    std::unique_ptr<stree::AbstractNode> m_stree;
    stree::ResourceNameSet m_rns;

    void prepareNamespace();
    void loadStree() throw(std::runtime_error);
};

}   //cdb
}   //nx

#endif  //NX_CDB_STREE_MANAGER_H
