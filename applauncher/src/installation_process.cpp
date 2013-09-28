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

#include <utils/common/log.h>

#include "mirror_list_xml_parse_handler.h"
#include "version.h"


InstallationProcess::InstallationProcess(
    const QString& productName,
    const QString& customization,
    const QString& version,
    const QString& module,
    const QString& installationDirectory )
:
    m_productName( productName ),
    m_customization( customization ),
    m_version( version ),
    m_module( module ),
    m_installationDirectory( installationDirectory ),
    m_state( State::init ),
    m_httpClient( nullptr ),
    m_status( applauncher::api::InstallationStatus::init )
{
    //TODO/IMPL
}

InstallationProcess::~InstallationProcess()
{
    pleaseStop();
    wait();
}

void InstallationProcess::pleaseStop()
{
    //TODO/IMPL
}

void InstallationProcess::wait()
{
    //TODO/IMPL
}

static const QString MIRROR_LIST_URL_PARAM_NAME( "mirrorListUrl" );
static const QString DEFAULT_MIRROR_LIST_URL( "http://networkoptix.com/archive/hdw_mirror_list.xml" );

bool InstallationProcess::start( const QSettings& settings )
{
    if( !m_httpClient )
    {
        m_httpClient = new nx_http::AsyncHttpClient();
        connect( m_httpClient, SIGNAL(done(nx_http::AsyncHttpClient*)), this, SLOT(onHttpDone(nx_http::AsyncHttpClient*)), Qt::DirectConnection );
    }

    m_state = State::downloadMirrorList;
    m_status = applauncher::api::InstallationStatus::inProgress;
    QString mirrorListUrl = settings.value(MIRROR_LIST_URL_PARAM_NAME, DEFAULT_MIRROR_LIST_URL).toString();
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
    //TODO/IMPL
    return m_state == State::finished ? 100 : 0;
}

QString InstallationProcess::errorText() const
{
    std::unique_lock<std::mutex> lk( m_mutex );
    return m_errorText;
}

void InstallationProcess::fileProgress(
    RDirSyncher* const syncher,
    int64_t remoteFileSize,
    int64_t bytesDownloaded )
{
    //TODO/IMPL
}

void InstallationProcess::fileDone(
    RDirSyncher* const syncher,
    const QString& filePath )
{
    //TODO/IMPL
}

void InstallationProcess::finished(
    RDirSyncher* const syncher,
    bool result )
{
    m_state = State::finished;
    m_status = result ? applauncher::api::InstallationStatus::success : applauncher::api::InstallationStatus::failed;
}

void InstallationProcess::failed(
    RDirSyncher* const syncher,
    const QString& failedFilePath,
    const QString& errorText )
{
    //TODO/IMPL
}

void InstallationProcess::onHttpDone( nx_http::AsyncHttpClient* httpClient )
{
    assert( m_httpClient == httpClient );

    auto scopedExitFunc = [this](InstallationProcess* /*pThis*/)
    {
        m_httpClient->terminate();
        m_httpClient->scheduleForRemoval();
        m_httpClient = nullptr;
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

    std::forward_list<QUrl> mirrorList;
    MirrorListXmlParseHandler xmlHandler(
        m_productName,
        m_customization,
        m_version,
        m_module,
        &mirrorList );

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

    if( mirrorList.empty() )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        m_errorText = QString::fromLatin1( "Could not find mirror for [%1; %2; %3; %4]" ).arg(m_productName).arg(m_customization).arg(m_module).arg(m_version);
        m_state = State::finished;
        m_status = applauncher::api::InstallationStatus::failed;
        return;
    }

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
