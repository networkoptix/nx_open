/**********************************************************
* May 4, 2016
* akolesnikov
***********************************************************/

#include "listening_peer.h"

#include <utils/common/model_functions.h>


namespace nx {
namespace hpm {
namespace data {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ListeningPeer)(ListeningPeerList)(ListeningPeersBySystem),
    (json),
    _Fields);

} // namespace data
} // namespace hpm
} // namespace nx
