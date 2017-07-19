#include "model.h"

namespace nx {
namespace cloud {
namespace relay {

Model::Model(const conf::Settings& settings):
    m_clientSessionPool(settings),
    m_listeningPeerPool(settings)
{
}

model::ClientSessionPool& Model::clientSessionPool()
{
    return m_clientSessionPool;
}

const model::ClientSessionPool& Model::clientSessionPool() const
{
    return m_clientSessionPool;
}

model::ListeningPeerPool& Model::listeningPeerPool()
{
    return m_listeningPeerPool;
}

const model::ListeningPeerPool& Model::listeningPeerPool() const
{
    return m_listeningPeerPool;
}

} // namespace relay
} // namespace cloud
} // namespace nx
