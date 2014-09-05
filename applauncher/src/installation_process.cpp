/**********************************************************
* 27 sep 2013
* a.kolesnikov
***********************************************************/

#include "installation_process.h"

#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtXml/QXmlInputSource>
#include <QtXml/QXmlSimpleReader>

#include <plugins/videodecoder/stree/streesaxhandler.h>
#include <utils/common/log.h>

#include "mirror_list_xml_parse_handler.h"
#include "version.h"


InstallationProcess::InstallationProcess(
    const QString& productName,
    const QString& customization,
    const QnSoftwareVersion &version,
    const QString& module,
    const QString& installationDirectory,
    bool autoStartNeeded )
:
    m_productName( productName ),
    m_customization( customization ),
    m_version( version ),
    m_module( module ),
    m_installationDirectory( installationDirectory ),
    m_state( State::init ),
    m_status( applauncher::api::InstallationStatus::init ),
    m_totalBytesDownloaded( 0 ),
    m_totalBytesToDownload( 0 ),
    m_autoStartNeeded( autoStartNeeded )
{
}

InstallationProcess::~InstallationProcess()
{
    pleaseStop();
    join();
}

void InstallationProcess::pleaseStop()
{
    //TODO/IMPL
}

void InstallationProcess::join()
{
    //TODO/IMPL
}

static const QString MIRROR_LIST_URL_PARAM_NAME( "mirrorListUrl" );
static const QString installationDataFile( "install.dat" );

bool InstallationProcess::start( const QString& mirrorListUrl )
{
    if( !m_httpClient )
    {
        m_httpClient.reset( new nx_http::AsyncHttpClient() );
        connect(
            m_httpClient.get(), &nx_http::AsyncHttpClient::done,
            this, &InstallationProcess::onHttpDone,
            Qt::DirectConnection );
    }

    m_fileSizeByEntry.clear();
    m_state = State::downloadMirrorList;
    m_status = applauncher::api::InstallationStatus::inProgress;
    if( !m_httpClient->doGet(QUrl(mirrorListUrl)) )
    {
        m_state = State::init;
        m_status = applauncher::api::InstallationStatus::failed;
        return false;
    }

    return true;
}

applauncher::api::InstallationStatus::Value InstallationProcess::getStatus() const
{
    std::unique_lock<std::mutex> lk( m_mutex );
    return m_status;
}

//!Returns installation progress (percents)
float InstallationProcess::getProgress() const
{
    std::unique_lock<std::mutex> lk( m_mutex );

    if( m_totalBytesToDownload == 0 )
        return -1;   //unknown

    int64_t totalBytesDownloaded = m_totalBytesDownloaded;
    for( auto fileBytesPair: m_unfinishedFilesBytesDownloaded )
        totalBytesDownloaded += fileBytesPair.second;
    const float percentCompleted = (float)totalBytesDownloaded / m_totalBytesToDownload * 100.0;
    return percentCompleted > 100.0 ? 100.0 : percentCompleted;
}

bool InstallationProcess::autoStartNeeded() const
{
    return m_autoStartNeeded;
}

QString InstallationProcess::errorText() const
{
    std::unique_lock<std::mutex> lk( m_mutex );
    return m_errorText;
}

QnSoftwareVersion InstallationProcess::getVersion() const
{
    return m_version;
}

void InstallationProcess::cancel()
{
    {
        std::unique_lock<std::mutex> lk( m_mutex );

        m_status = applauncher::api::InstallationStatus::cancelInProgress;

        if (m_httpClient) {
            m_httpClient->terminate();
            m_httpClient.reset();
        }

        if (m_state == State::installing)
            m_syncher->cancel();

        m_status = applauncher::api::InstallationStatus::cancelled;
    }

    emit installationDone(this);
}

void InstallationProcess::overrallDownloadSizeKnown(
    const std::shared_ptr<RDirSyncher>& /*syncher*/,
    int64_t totalBytesToDownload )
{
    m_totalBytesToDownload = totalBytesToDownload;
}

void InstallationProcess::fileProgress(
    const std::shared_ptr<RDirSyncher>& /*syncher*/,
    const QString& filePath,
    int64_t /*remoteFileSize*/,
    int64_t bytesDownloaded )
{
    //NOTE multiple files can be downloaded simultaneously
    std::unique_lock<std::mutex> lk( m_mutex );
    m_unfinishedFilesBytesDownloaded[filePath] = bytesDownloaded;
}

void InstallationProcess::fileDone(
    const std::shared_ptr<RDirSyncher>& /*syncher*/,
    const QString& filePath )
{
    std::unique_lock<std::mutex> lk( m_mutex );

    m_fileSizeByEntry.insert(filePath, QFile(m_installationDirectory + "/" + filePath).size());

    auto it = m_unfinishedFilesBytesDownloaded.find( filePath );
    if( it == m_unfinishedFilesBytesDownloaded.end() )
        return;

    m_totalBytesDownloaded += it->second;
    m_unfinishedFilesBytesDownloaded.erase( it );
}

void InstallationProcess::finished(
    const std::shared_ptr<RDirSyncher>& /*syncher*/,
    bool result )
{
    m_state = State::finished;
    if( result && writeInstallationSummary() ) {
        m_status = applauncher::api::InstallationStatus::success;
    } else {
        m_status = applauncher::api::InstallationStatus::failed;
    }

    emit installationDone( this );
}

void InstallationProcess::failed(
    const std::shared_ptr<RDirSyncher>& /*syncher*/,
    const QString& /*failedFilePath*/,
    const QString& /*errorText*/ )
{
    //TODO/IMPL
}

bool InstallationProcess::writeInstallationSummary()
{
    QFile file(m_installationDirectory + "/" + installationDataFile);
    if (!file.open(QFile::WriteOnly))
        return false;

    QDataStream stream(&file);
    stream << m_fileSizeByEntry;
    file.close();

    return true;
}

void InstallationProcess::onHttpDone( nx_http::AsyncHttpClientPtr httpClient )
{
    assert( m_httpClient == httpClient );

    auto scopedExitFunc = [this](InstallationProcess* /*pThis*/)
    {
        m_httpClient->terminate();
        m_httpClient.reset();
        if( m_status == applauncher::api::InstallationStatus::failed )
        {
            NX_LOG( m_errorText, cl_logERROR );
        }
    };
    std::unique_ptr<InstallationProcess, decltype(scopedExitFunc)> removeHttpClientGuard( this, scopedExitFunc );

    if( (m_httpClient->state() != nx_http::AsyncHttpClient::sDone) ||
        !m_httpClient->response() )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        m_errorText = QString::fromLatin1("Failed to download %1. I/O issue").arg(m_httpClient->url().toString());
        m_state = State::finished;
        m_status = applauncher::api::InstallationStatus::failed;
        return;
    }

    if( m_httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        m_errorText = QString::fromLatin1("Failed to download %1. %2").arg(m_httpClient->url().toString()).
            arg(QLatin1String(nx_http::StatusCode::toString(m_httpClient->response()->statusLine.statusCode)));
        m_state = State::finished;
        m_status = applauncher::api::InstallationStatus::failed;
        return;
    }

    //reading message body and parsing mirror list
    nx_http::BufferType msgBody = m_httpClient->fetchMessageBodyBuffer();

    stree::SaxHandler xmlHandler( m_rns );

    QXmlSimpleReader reader;
    reader.setContentHandler( &xmlHandler );
    reader.setErrorHandler( &xmlHandler );

    QBuffer xmlFile( &msgBody );
    if( !xmlFile.open( QIODevice::ReadOnly ) )
        assert( false );
    QXmlInputSource input( &xmlFile );
    if( !reader.parse( &input ) )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        m_errorText = QString::fromLatin1( "Failed to parse mirror_list.xml received from %1. %2" ).arg(m_httpClient->url().toString()).arg(xmlHandler.errorString());
        m_state = State::finished;
        m_status = applauncher::api::InstallationStatus::failed;
        return;
    }
    m_currentTree.reset( xmlHandler.releaseTree() );

    stree::ResourceContainer inputData;
    inputData.put( ProductParameters::product, m_productName );
    inputData.put( ProductParameters::customization, m_customization );
    inputData.put( ProductParameters::module, m_module );
    inputData.put( ProductParameters::version, m_version.toString(QnSoftwareVersion::MinorFormat) );
#ifdef _MSC_VER
#ifdef _WIN64
    inputData.put( ProductParameters::arch, "x64" );
#else
    inputData.put( ProductParameters::arch, "x86" );
#endif
#elif defined(__GNUC__)
#ifdef __x86_64
    inputData.put( ProductParameters::arch, "x64" );
#else
    inputData.put( ProductParameters::arch, "x86" );
#endif
#else
#error "Unknown compiler"
#endif

#ifdef _WIN32
    inputData.put( ProductParameters::platform, "windows" );
#elif defined(__linux__)
    inputData.put( ProductParameters::platform, "linux" );
#elif defined(__APPLE__)
    inputData.put( ProductParameters::platform, "macos" );
#endif


    stree::ResourceContainer result;
    NX_LOG( QString::fromLatin1("Searching mirrors.xml with following input data: %1").arg(inputData.toString(m_rns)), cl_logDEBUG2 );
    m_currentTree->get( inputData, &result );

    std::forward_list<QUrl> mirrorList;
    {
        QString urlStr;
        for( result.goToBeginning(); result.next(); )
            if( result.resID() == ProductParameters::mirrorUrl )
                mirrorList.push_front( QUrl(result.value().toString()) );
    }

    if( mirrorList.empty() )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        m_errorText = QString::fromLatin1( "Could not find mirror for %1" ).arg(inputData.toString(m_rns));
        m_state = State::finished;
        m_status = applauncher::api::InstallationStatus::failed;
        return;
    }

    QFileInfo fileInfo(m_installationDirectory);
    if (fileInfo.exists() && !fileInfo.isDir()) // if there is a ghost file remove it
        QFile::remove(fileInfo.absolutePath());

    if( !QDir().mkpath(m_installationDirectory) )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        m_errorText = QString::fromLatin1( "Failed to start installation from %1 to %2. Cannot create target directory" ).
            arg(mirrorList.front().toString()).arg(m_installationDirectory);
        m_state = State::finished;
        m_status = applauncher::api::InstallationStatus::failed;
        return;
    }

    m_state = State::installing;

    m_syncher.reset( new RDirSyncher( mirrorList, m_installationDirectory, this ) );
    //syncher->setFileProgressNotificationStep(  );

    if( !m_syncher->startAsync() )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        m_errorText = QString::fromLatin1( "Failed to start installation from %1 to %2. %3" ).
            arg(mirrorList.front().toString()).arg(m_installationDirectory).arg(m_syncher->lastErrorText());
        m_syncher.reset();
        m_state = State::finished;
        m_status = applauncher::api::InstallationStatus::failed;
    }
}
