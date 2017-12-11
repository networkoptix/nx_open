#include "by_platform_peer_selector.h"

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {
namespace peer_selection {
namespace impl {

namespace {
class SimilarPeersSelector
{
public:
    SimilarPeersSelector(
        const QnSystemInformation& selfInformation,
        const PeerInformationList& otherPeerInfos)
        :
        m_selfInformation(selfInformation)
    {
        for (const auto& peerInfo: otherPeerInfos)
            addId(peerInfo);
    }

    QList<QnUuid> take() { return std::move(m_result); }
private:
    const QnSystemInformation& m_selfInformation;
    QList<QnUuid> m_result;

    void addId(const PeerInformation& peerInfo)
    {
        const bool isInfoSuitable = peerInfo.systemInformation.arch == m_selfInformation.arch
            && peerInfo.systemInformation.platform == m_selfInformation.platform
            && peerInfo.systemInformation.modification == m_selfInformation.modification;

        if (!isInfoSuitable)
            return;

        m_result.append(peerInfo.id);
    }
};
} // namespace

ByPlatformPeerSelector::ByPlatformPeerSelector(const QnSystemInformation& selfInformation):
    m_selfInformation(selfInformation)
{}

QList<QnUuid> ByPlatformPeerSelector::peers(const PeerInformationList& allOtherPeers) const
{
    return SimilarPeersSelector(m_selfInformation, allOtherPeers).take();
}

AbstractPeerSelectorPtr ByPlatformPeerSelector::create(
    const QnSystemInformation& systemInformation)
{
    return std::make_unique<ByPlatformPeerSelector>(systemInformation);
}

} // namespace impl
} //namespace peer_selection
} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx