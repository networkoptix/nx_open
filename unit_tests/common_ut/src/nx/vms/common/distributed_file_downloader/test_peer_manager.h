#pragma once

#include <QtCore/QHash>

#include <nx/vms/common/private/distributed_file_downloader/abstract_peer_manager.h>

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

class TestPeerManager: public AbstractPeerManager
{
public:
    TestPeerManager();

    void addPeer(const QnUuid& peer);
    void setFileInformation(const QnUuid& peer, const DownloaderFileInformation& fileInformation);

    // AbstractPeerManager implementation

    virtual QList<QnUuid> getAllPeers() const override;

    virtual rest::Handle requestFileInfo(
        const QnUuid& peer,
        const QString& fileName,
        FileInfoCallback callback) override;

    virtual void cancelRequest(const QnUuid& peerId, rest::Handle handle) override;

private:
    struct PeerInfo
    {
        QHash<QString, DownloaderFileInformation> fileInformationByName;
    };

    QHash<QnUuid, PeerInfo> m_peers;
    int m_requestIndex = 0;
};

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
