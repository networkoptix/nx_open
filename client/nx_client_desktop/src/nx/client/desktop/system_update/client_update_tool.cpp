#include <utils/applauncher_utils.h>
#include <nx/utils/log/log.h>
#include <nx/vms/common/p2p/downloader/private/abstract_peer_manager.h>
#include <nx/vms/common/p2p/downloader/private/resource_pool_peer_manager.h>
#include <nx/vms/common/p2p/downloader/private/peer_selection/peer_selector_factory.h>

#include "client_update_tool.h"

namespace nx::vms::common::p2p::downloader {

// This is a hacky class to make P2P downloader work for client as well.
// It should be removed when P2P downloader is properly fixed.
class PeerManagerFactory:
    public AbstractPeerManagerFactory,
    public QnCommonModuleAware
{
public:
    PeerManagerFactory(QnCommonModule* commonModule):
        QnCommonModuleAware(commonModule)
    {

    }

    virtual AbstractPeerManager* createPeerManager(
        FileInformation::PeerSelectionPolicy peerPolicy,
        const QList<QnUuid>& additionalPeers) override
    {
        auto selector = peer_selection::PeerSelectorFactory::create(peerPolicy, additionalPeers, commonModule());
        return new ResourcePoolPeerManager(commonModule(), std::move(selector), true);
    }
};

} // namespace nx::vms::common::p2p::downloader

namespace nx {
namespace client {
namespace desktop {

ClientUpdateTool::ClientUpdateTool(QObject *parent):
    base_type(parent),
    m_outputDir(QDir::temp().absoluteFilePath("nx_updates/client"))
{
    // Expecting m_outputDir to be like /temp/nx_updates/client

    m_peerManagerFactory.reset(new nx::vms::common::p2p::downloader::PeerManagerFactory(commonModule()));
    m_downloader.reset(new Downloader(QDir("downloads"), commonModule(), m_peerManagerFactory.get()));
    connect(m_downloader.get(), &Downloader::fileStatusChanged,
            this, &ClientUpdateTool::at_downloaderStatusChanged);
}

ClientUpdateTool::~ClientUpdateTool()
{

}

void ClientUpdateTool::setState(State newState)
{
    if (m_state != newState)
    {
        m_state = newState;
        m_stateChanged = true;
    }
    m_lastError = "";
}

void ClientUpdateTool::setError(QString error)
{
    m_state = State::error;
    m_lastError = error;
}

void ClientUpdateTool::setApplauncherError(QString error)
{
    m_state = State::applauncherError;
    m_lastError = error;
}

void ClientUpdateTool::downloadUpdate(const UpdateContents& contents)
{
    NX_VERBOSE(this) << "downloadUpdate() ver" << contents.info.version;
    m_clientPackage = contents.clientPackage;
    m_updateVersion = nx::utils::SoftwareVersion(contents.info.version);

    if (contents.sourceType == UpdateSourceType::file)
    {
        // Expecting that file is stored at:
        QString path = contents.storageDir.filePath(m_clientPackage.file);
        //unpackUpdate(path);
        m_updateFile = path;
        setState(State::readyInstall);
    }
    else
    {
        // TODO: We already have a URL to client update. We should download and install it directly.
        FileInformation info;
        info.md5 = QByteArray::fromHex(m_clientPackage.md5.toLatin1());
        info.size = m_clientPackage.size;
        info.name = m_clientPackage.file;
        info.url = m_clientPackage.url;
        auto code = m_downloader->addFile(info);
        using Code = vms::common::p2p::downloader::ResultCode;
        m_updateFile =  m_downloader->filePath(m_clientPackage.file);

        switch (code)
        {
            case Code::ok:
                NX_VERBOSE(this) << "requestStartUpdate() - downloading client package"
                    << info.name;
                setState(State::downloading);
                break;
            case Code::fileAlreadyExists:
                NX_VERBOSE(this) << "requestStartUpdate() - file is already here"
                    << info.name;
                setState(State::readyInstall);
                break;
            default:
                // TODO: Some sort of an error here
                {
                    QString error = vms::common::p2p::downloader::toString(code);
                    NX_VERBOSE(this) << "requestStartUpdate() - failed to add client package "
                        << info.name << error;
                    setError(error);
                    break;
                }
        }
    }
    /*
     * Client update consists of:
     * 1. Download client package
     * 2. Unpack client package
     * 3. Verify unpacked contents
     * 4. Install it.
     */
}

void ClientUpdateTool::at_downloaderStatusChanged(const FileInformation& fileInformation)
{
    if (fileInformation.name != m_clientPackage.file)
        return;

    NX_VERBOSE(this) << "at_downloaderStatusChanged("<< fileInformation.name
             << ") - status changed to " << fileInformation.status;

    if (m_state != State::downloading)
    {
        // WTF are we doing here?
        return;
    }

    switch (fileInformation.status)
    {
        case FileInformation::Status::notFound:
            setError(tr("Update file is not found"));
            break;
        case FileInformation::Status::uploading:
            break;
        case FileInformation::Status::downloaded:
            setState(State::readyInstall);
            //unpackUpdate(fileInformation.name);
            break;
        case FileInformation::Status::corrupted:
            setError(tr("Update package is corrupted"));
            break;
        case FileInformation::Status::downloading:
            m_progress = 0;//fileInformation.downloadedChunks
        default:
            // Nothing to do here
            break;
    }
}

#ifdef TO_BE_REMOVED
void ClientUpdateTool::unpackUpdate(QString packagePath)
{
    NX_INFO(this) << "unpackUpdate" + packagePath;

    m_updateFile = packagePath;

    if (m_extractor)
    {
        // TODO: Why do we have an extractor instance?
        disconnect(m_extractor.get(), &QnZipExtractor::finished, this, &ClientUpdateTool::at_extractFilesFinished);
    }

    // Clean up existing folder for updates.
    m_outputDir.removeRecursively();
    if (!m_outputDir.exists())
        m_outputDir.mkpath(".");

    m_extractor = std::make_shared<QnZipExtractor>(packagePath, m_outputDir.path());
    connect(m_extractor.get(), &QnZipExtractor::finished, this, &ClientUpdateTool::at_extractFilesFinished);
    setState(State::unpacking);
    m_extractor->start();
}
#endif

void ClientUpdateTool::at_extractFilesFinished(int code)
{
    if (code != QnZipExtractor::Ok)
    {
        QString error = QnZipExtractor::errorToString((QnZipExtractor::Error)code);
        NX_VERBOSE(this) << "at_extractFilesFinished() err=" << error;
        setError(tr("Update package is corrupted: %1").arg(error));
        return;
    }
    NX_VERBOSE(this) << "at_extractFilesFinished() done unpacking file" << error;
    setState(State::readyInstall);
}

int ClientUpdateTool::getDownloadProgress() const
{
    if (m_state == State::readyInstall)
        return 100;
    return m_progress;
}

bool ClientUpdateTool::isDownloadComplete() const
{
    return !hasUpdate() || m_state == State::readyInstall;
}

bool ClientUpdateTool::installUpdate()
{

    /* Try to run applauncher if it is not running. */
    if (!applauncher::api::checkOnline())
    {
        NX_VERBOSE(this) << "installUpdate can not install update - applauncher is offline" << error;
        setApplauncherError("applauncher is offline");
        return false;
    }

    NX_ASSERT(!m_updateFile.isEmpty());

    static const int kMaxTries = 5;
    QString absolutePath = QFileInfo(m_updateFile).absoluteFilePath();

    for (int retries = 0; retries < kMaxTries; retries++)
    {
        using Result = applauncher::api::ResultType::Value;
        Result result = applauncher::api::installZip(m_updateVersion, absolutePath);

        switch (result)
        {
            case Result::alreadyInstalled:
                setState(State::complete);
                return true;

            case Result::ok:
                setState(State::installing);
                return true;

            case Result::otherError:
            case Result::versionNotInstalled:
            case Result::invalidVersionFormat:
            {
                QString error = applauncher::api::ResultType::toString(result);
                NX_ERROR(this) << "Failed to run installation:" << error;
                setApplauncherError(error);
                return false;
            }
            default:
                // Other variats can be fixed by retrying installation, do they?
                break;
        }

        QThread::msleep(100);
        qApp->processEvents();
    }
    return false;
}

bool ClientUpdateTool::isInstallComplete() const
{
    switch (m_state)
    {
        // We have installation progress here.
        case State::readyInstall:
        case State::downloading:
            return false;
        case State::installing:
            // We need to check applauncher to get a proper result
            break;
        case State::complete:
        case State::initial:
        // Though actual install is not successful, no further progress is possible.
        case State::error:
        case State::applauncherError:
            return true;
    }

    bool installed = false;
    using Result = applauncher::api::ResultType::Value;
    Result result = applauncher::api::isVersionInstalled(m_updateVersion, &installed);

    switch (result)
    {
        case Result::alreadyInstalled:
            break;
        case Result::ok:
            return true;

        case Result::otherError:
        case Result::versionNotInstalled:
        case Result::invalidVersionFormat:
        {
            QString error = applauncher::api::ResultType::toString(result);
            NX_ERROR(this) << "Failed to check installation:" << error;
            //setApplauncherError(error);
            return false;
        }
        default:
            // Other variats can be fixed by retrying installation, do they?
            break;
    }

    return installed;
}

bool ClientUpdateTool::restartClient()
{
    using namespace applauncher::api;

    /* Try to run applauncher if it is not running. */
    if (!applauncher::api::checkOnline())
        return false;

    const auto result = applauncher::api::restartClient(m_updateVersion);
    if (result == ResultType::ok)
        return true;

    static const int kMaxTries = 5;
    for (int i = 0; i < kMaxTries; ++i)
    {
        QThread::msleep(100);
        qApp->processEvents();
        if (applauncher::api::restartClient(m_updateVersion) == ResultType::ok)
            return true;
    }
    return false;
}

void ClientUpdateTool::resetState()
{
    // TODO: Cancel download if there is any.
    // TODO: Reset current update info.
    switch (m_state)
    {
        case State::downloading:
            // TODO: Cancel it
            break;
        //case State::unpacking:
        //    break;
        case State::readyInstall:
            break;
        case State::initial:
        default:
            // Nothing to do here
            break;
    }

    m_state = State::initial;
    m_lastError = "";
    m_updateFile = "";
    m_progress = 0;
    m_updateVersion = nx::utils::SoftwareVersion();
}

ClientUpdateTool::State ClientUpdateTool::getState() const
{
    return m_state;
}

bool ClientUpdateTool::hasUpdate() const
{
    return m_state != State::initial && m_state != State::error && m_state != State::applauncherError;
}

} // namespace desktop
} // namespace client
} // namespace nx
