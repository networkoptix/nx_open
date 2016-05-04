/**********************************************************
* May 4, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <map>
#include <vector>

#include <QtCore/QString>

#include <nx/network/buffer.h>

#include <utils/common/model_functions_fwd.h>


namespace nx {
namespace hpm {
namespace data {

struct ListeningPeer
{
    QString id;
    QString endpoint;
};

#define ListeningPeer_Fields (id)(endpoint)


struct ListeningPeerList
{
    std::vector<ListeningPeer> peers;
};

#define ListeningPeerList_Fields (peers)


struct ListeningPeersBySystem
{
    /** system id,  */
    std::map<QString, ListeningPeerList> systems;
};

#define ListeningPeersBySystem_Fields (systems)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (ListeningPeer)(ListeningPeerList)(ListeningPeersBySystem),
    (json));

} // namespace data
} // namespace hpm
} // namespace nx
