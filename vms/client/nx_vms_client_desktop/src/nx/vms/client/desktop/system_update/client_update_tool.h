// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <nx/vms/client/core/network/connection_info.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/client/desktop/system_update/update_contents.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/vms/common/p2p/downloader/private/abstract_peer_manager.h>
#include <nx/vms/common/update/update_check_params.h>
#include <nx/vms/common/update/update_information.h>
#include <nx/vms/common/utils/file_signature.h>

namespace nx::vms::applauncher::api { enum class ResultType; }

namespace nx::vms::client::core {
struct LogonData;
class SystemContext;
} // namespace nx::vms::client::core

namespace nx::vms::common { class AbstractCertificateVerifier; }

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
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;
    using Downloader = vms::common::p2p::downloader::Downloader;
    using FileInformation = vms::common::p2p::downloader::FileInformation;
    using PeerManagerPtr = nx::vms::common::p2p::downloader::AbstractPeerManager*;

public:
    ClientUpdateTool(core::SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~ClientUpdateTool() override;

    using Error = std::variant<nx::vms::common::update::Status::ErrorCode, QString>;

    enum class State
    {
        /** Tool starts at this state. */
        initial,
        /** Waiting for mediaserver's response with current update info. */
        pendingUpdateInfo,
        /** Got update info and ready to download. */
        readyDownload,
        /** Downloading client package. */
        downloading,
        /** Verifying client package. */
        verifying,
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
     * There can be some errors.
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
     * @param logonData Target System to connect to and its credentials.
     * @return true if command was successful.
     */
    bool restartClient(std::optional<nx::vms::client::core::LogonData> logonData);

    /**
     * Check if client should be restarted to this version.
     * @return True if it is really needed.
     */
    bool shouldRestartTo(const nx::utils::SoftwareVersion& version) const;

    /**
     * Updates URL of the current mediaserver.
     */
    void setServerUrl(
        const nx::vms::client::core::LogonData& logonData,
        nx::vms::common::AbstractCertificateVerifier* certificateVerifier);

    /**
     * Requests update information from the Internet. It can proxy request through mediaserver if
     * client has no internet.
     */
    std::future<UpdateContents> requestInternetUpdateInfo(
        const nx::utils::Url& updateUrl, const nx::vms::update::PublicationInfoParams& params);

    std::future<UpdateContents> requestUpdateInfoFromServer(
        const nx::vms::common::update::UpdateInfoParams& params);

    /**
     * Get installed versions. It can block up to 500ms until applauncher check it complete.
     * The check is initiated when ClientUpdateTool is created, so there will be no block
     * most of the time.
     * @param includeCurrentVersion Should we include a version of current client instance.
     */
    std::set<nx::utils::SoftwareVersion> getInstalledClientVersions(bool includeCurrentVersion = false) const;

    struct SystemServersInfo
    {
        QnUuidList serversWithInternet;
        QnUuidList persistentStorageServers;
    };

    /**
     * Get information about servers in the target system: where update files are stored, which
     * servers have internet connection.
     * @param targetVersion Version of servers in the system. Needed to choose proper API calls.
     */
    ClientUpdateTool::SystemServersInfo getSystemServersInfo(
        const nx::utils::SoftwareVersion& targetVersion) const;
    void setupProxyConnections(const SystemServersInfo& serversInfo);

    static QString toString(State state);

    Error getError() const { return m_error; }
    static QString errorString(const Error& error);

    void checkInternalState();

signals:
    /**
     * Emitted every time update state changes, or its internal state is updated.
     * @param state Current state, corresponds to enum ClientUpdateTool::State.
     * @param percentComplete Progress for this state.
     * @param error Error code or printable error message.
     */
    void updateStateChanged(nx::vms::client::desktop::ClientUpdateTool::State state,
        int percentComplete,
        const nx::vms::client::desktop::ClientUpdateTool::Error& error);

protected:
    // Callbacks
    void atDownloaderStatusChanged(const FileInformation& fileInformation);
    void atDownloadFinished(const QString& fileName);
    void atChunkDownloadFailed(const QString& fileName);
    void atDownloadFailed(const QString& fileName);
    void atDownloadStallChanged(const QString& fileName, bool stalled);
    void atVerificationFinished();

private:
    void setState(State newState);
    void setError(const Error& error);
    void clearDownloadFolder();
    void verifyUpdateFile();

    /**
     * Converts applauncher::api::ResultType to a readable string.
     */
    static QString applauncherErrorToString(nx::vms::applauncher::api::ResultType result);
    static QString downloaderErrorToString(nx::vms::common::p2p::downloader::ResultCode result);

    std::unique_ptr<Downloader> m_downloader;
    /** Directory to store unpacked files. */
    QDir m_outputDir;

    vms::common::p2p::downloader::ResourcePoolPeerManager* m_peerManager = nullptr;
    vms::common::p2p::downloader::ResourcePoolPeerManager* m_proxyPeerManager = nullptr;
    nx::vms::update::Package m_clientPackage;

    std::atomic<State> m_state = State::initial;

    bool m_stateChanged = false;
    nx::utils::SoftwareVersion m_updateVersion;

    /**
     * Direct connection to the mediaserver.
     * It is used to request update manifest in compatibility mode.
     */
    rest::ServerConnectionPtr m_serverConnection;
    /** Full path to update package. */
    QString m_updateFile;
    Error m_error;

    nx::Mutex m_mutex;

    std::future<nx::vms::applauncher::api::ResultType> m_applauncherTask;
    std::future<nx::vms::common::FileSignature::Result> m_verificationResult;

    mutable std::future<std::set<nx::utils::SoftwareVersion>> m_installedVersionsFuture;
    mutable std::set<nx::utils::SoftwareVersion> m_installedVersions;
};

} // namespace nx::vms::client::desktop
