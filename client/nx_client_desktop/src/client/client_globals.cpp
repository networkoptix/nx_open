#include "client_globals.h"

namespace Qn {

bool isSeparatorNode(NodeType t)
{
    return t == SeparatorNode
        || t == LocalSeparatorNode
        ;
}

} //namespace Qn
