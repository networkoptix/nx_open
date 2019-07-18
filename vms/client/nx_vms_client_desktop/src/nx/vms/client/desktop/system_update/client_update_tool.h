#pragma once

#include <set>

#include <client_core/connection_context_aware.h>

#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/vms/common/p2p/downloader/private/abstract_peer_manager.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/update/update_information.h>
#include <utils/common/connective.h>
#include <utils/update/zip_utils.h>

namespace nx::vms::applauncher::api { enum class ResultType; }

namespace nx::vms::common::p2p::downloader {

class ResourcePoolPeerManager;

} // namespace nx::vms::common::p2p::downloader

namespace nx::vms::client::desktop {

/**
 * A tool to deal with client updates.
 * Widget sets UpdateContents to it and waits until package is downloaded and installed.
 * ClientUpdateTool uses p2p downloader to get update package.
 */
class ClientUpdateTool:
    public Connective<QObject>,
    public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;
    using Downloader = vms::common::p2p::downloader::Downloader;
    using FileInformation = vms::common::p2p::downloader::FileInformation;
    using PeerManagerPtr = nx::vms::common::p2p::downloader::AbstractPeerManager*;
    using UpdateContents = nx::update::UpdateContents;

public:
    ClientUpdateTool(QObject* parent = nullptr);
    virtual ~ClientUpdateTool() override;

    enum State
    {
        /** Tool starts at this state. */
        initial,
        /** Waiting for mediaserver's response with current update info. */
        pendingUpdateInfo,
        /** Got update info and ready to download. */
        readyDownload,
        /** Downloading client package. */
        downloading,
        /** Ready to install client package. */
        readyInstall,
        /** Installing update using applauncher. */
        installing,
        /** Client is ready to reboot to new version. */
        readyRestart,
        /** Installation is complete, client has newest version. */
        complete,
        /** ClientUpdateTool is being destructed. All async operations should exit ASAP. */
        exiting,
        /** Got some critical error and can not continue installation. */
        error,
        /** Got an error during installation. */
        applauncherError,
    };

    /**
     * Simple check whether client should install anything from the contents.
     * @param contents
     * @return true if client's update package should be installed.
     */
    bool shouldInstallThis(const UpdateContents& contents) const;

    /**
     * Checks if this client version is already installed.
     */
    bool isVersionInstalled(const nx::utils::SoftwareVersion& version) const;

    /**
     * Asks tool to get update for client.
     * If client should download update - it will start downloading.
     * If client already has this update - it goes to readyReboot.
     * @param info - update manifest
     */
    void setUpdateTarget(const UpdateContents& contents);

    /**
     * Resets tool to initial state.
     */
    void resetState();

    /**
     * Check if we have an update to install.
     */
    bool hasUpdate() const;

    /**
     * Check if download is complete.
     * It will return false if there was no update.
     */
    bool isDownloadComplete() const;

    /**
     * Check if installation is complete.
     * There can be some awful errors.
     */
    bool isInstallComplete() const;

    State getState() const;

    /**
     * Tells applauncher to install update package.
     * It will work only if tool is in state State::readyInstall. This is asyncronous call.
     * You should subscribe to 'updateStateChanged' to get notification.
     * @return true if success
     */
    bool installUpdateAsync();

    /**
     * Restart client to the new version.
     * Works only in state State::complete.
     * @return true if command was successful.
     */
    bool restartClient(QString authString = "");

    /**
     * Check if client should be restarted to this version.
     * @return True if it is really needed.
     */
    bool shouldRestartTo(const nx::utils::SoftwareVersion& version) const;

    /**
     * Updates URL of the current mediaserver.
     */
    void setServerUrl(const nx::utils::Url& serverUrl, const QnUuid& serverId);

    std::future<UpdateContents> requestRemoteUpdateInfo();

    /**
     * Get installed versions. It can block up to 500ms until applauncher check it complete.
     * The check is initiated when ClientUpdateTool is created, so there will be no block
     * most of the time.
     * @param includeCurrentVersion Should we include a version of current client instance.
     */
    std::set<nx::utils::SoftwareVersion> getInstalledVersions(bool includeCurrentVersion = false) const;

    static QString toString(State state);

    /**
     * Get readable error text.
     */
    QString getErrorText() const;

    void checkInternalState();

signals:
    /**
     * This event is emitted every time update state changes,
     * or its internal state is updated.
     * @param state - current state, corresponds to enum ClientUpdateTool::State;
     * @param percentComplete - progress for this state.
     */
    void updateStateChanged(int state, int percentComplete);

protected:
    // Callbacks
    void atDownloaderStatusChanged(const FileInformation& fileInformation);
    void atDownloadFinished(const QString& fileName);
    void atChunkDownloadFailed(const QString& fileName);
    void atDownloadFailed(const QString& fileName);
    void atDownloadStallChanged(const QString& fileName, bool stalled);
    void atExtractFilesFinished(int code);

private:
    void setState(State newState);
    void setError(const QString& error);
    void setApplauncherError(const QString& error);
    void clearDownloadFolder();
    /**
     * Converts applauncher::api::ResultType to a readable string.
     */
    static QString applauncherErrorToString(nx::vms::applauncher::api::ResultType result);

    std::unique_ptr<Downloader> m_downloader;
    /** Directory to store unpacked files. */
    QDir m_outputDir;

    vms::common::p2p::downloader::ResourcePoolPeerManager* m_peerManager = nullptr;
    vms::common::p2p::downloader::ResourcePoolPeerManager* m_proxyPeerManager = nullptr;
    nx::update::Package m_clientPackage;

    std::atomic<State> m_state = State::initial;

    bool m_stateChanged = false;
    nx::utils::SoftwareVersion m_updateVersion;

    /** Direct connection to the mediaserver. */
    rest::QnConnectionPtr m_serverConnection;
    /** Full path to update package. */
    QString m_updateFile;
    QString m_lastError;

    std::promise<UpdateContents> m_remoteUpdateInfoRequest;
    QnMutex m_mutex;

    std::future<nx::vms::applauncher::api::ResultType> m_applauncherTask;

    mutable std::future<std::set<nx::utils::SoftwareVersion>> m_installedVersionsFuture;
    mutable std::set<nx::utils::SoftwareVersion> m_installedVersions;
};

} // namespace nx::vms::client::desktop
