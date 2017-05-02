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
    struct FileInformation: DownloaderFileInformation
    {
        using DownloaderFileInformation::DownloaderFileInformation;
        QString filePath;
    };

    TestPeerManager();

    void addPeer(const QnUuid& peer);
    QnUuid addPeer();

    void setFileInformation(const QnUuid& peer, const FileInformation& fileInformation);
    FileInformation fileInformation(const QnUuid& peer, const QString& fileName) const;

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
        QHash<QString, FileInformation> fileInformationByName;
    };

    QHash<QnUuid, PeerInfo> m_peers;
    int m_requestIndex = 0;
};

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
