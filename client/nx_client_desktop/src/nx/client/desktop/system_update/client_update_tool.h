#pragma once

#include <QtCore>

#include <client_core/connection_context_aware.h>

#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/update/update_information.h>
#include <utils/common/connective.h>
#include <utils/update/zip_utils.h>

#include "update_contents.h"

namespace nx {
namespace client {
namespace desktop {

/**
 * A tool to deal with client updates.
 * Widget sets UpdateContents to it and waits until
 * it is downloaded and installed.
 * ClientUpdateTool uses p2p downloader to get update package.
 */
class ClientUpdateTool: public Connective<QObject>,
        public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;
    using Downloader = vms::common::p2p::downloader::Downloader;
    using FileInformation = vms::common::p2p::downloader::FileInformation;

public:
    ClientUpdateTool(QObject* parent = nullptr);
    ~ClientUpdateTool();

    enum State
    {
        initial,
        // Downloading client package.
        downloading,
        // Ready to install client package.
        readyInstall,
        // Installing update using applauncher.
        installing,
        // Installation is complete, client has newest version.
        complete,
        // Got some critical error and can not continue installation.
        error,
        // Got an error during installation.
        applauncherError,
    };

    /**
     * Asks downloader to get update for client.
     * @param info - update manifest
     */
    void downloadUpdate(const UpdateContents& contents);

    /**
     * Returns a progress for downloading client package
     * @return percents
     */
    int getDownloadProgress() const;

    /**
     * Resets tool to initial state.
     */
    void resetState();

    // Check if we have an update to install.
    bool hasUpdate() const;

    // Check if download is complete.
    // It will return false if there was no update.
    bool isDownloadComplete() const;

    // Check if installation is complete.
    // There can be some awful errors.
    bool isInstallComplete() const;

    State getState() const;

    /**
     * Tells applauncher to install update package.
     * @param version - version to be sent to applauncher
     * @return true if success
     */
    bool installUpdate();

    /**
     * Restart client to the new version.
     * Works only in state State::complete.
     * @return true if command was successful.
     */
    bool restartClient();

protected:
    void at_downloaderStatusChanged(const FileInformation& fileInformation);
    void at_extractFilesFinished(int code);
    void setState(State newState);
    void setError(QString error);
    void setApplauncherError(QString error);

protected:
    std::unique_ptr<Downloader> m_downloader;
    // Directory to store unpacked files.
    QDir m_outputDir;
    // Downloader needs this strange thing.
    std::unique_ptr<vms::common::p2p::downloader::AbstractPeerManagerFactory> m_peerManagerFactory;
    nx::update::Package m_clientPackage;
    State m_state = State::initial;
    int m_progress = 0;
    bool m_stateChanged = false;
    nx::utils::SoftwareVersion m_updateVersion;


    // Full path to update package.
    QString m_updateFile;

    // TODO: Actually, we do not need extractor. Applauncher deals with it.
    //std::shared_ptr<QnZipExtractor> m_extractor;
    QString m_lastError;
};

} // namespace desktop
} // namespace client
} // namespace nx
