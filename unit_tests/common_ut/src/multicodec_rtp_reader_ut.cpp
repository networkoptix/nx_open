/**********************************************************
* 24 feb 2015
* a.kolesnikov
***********************************************************/

#if 1

#include <fcntl.h>
#ifndef _WIN32
#include <poll.h>
#include <unistd.h>
#include <sys/uio.h>
#endif

#include <condition_variable>
#include <iostream>
#include <iomanip>
#include <memory>
#include <mutex>

#include <gtest/gtest.h>

#include <QtCore/QUuid>

#include <core/resource/network_resource.h>
#include <core/resource_management/resource_properties.h>
#include <nx/utils/std/cpp14.h>
#include <utils/common/synctime.h>
#include <network/multicodec_rtp_reader.h>
#include <nx/streaming/rtsp_client.h>
#include <nx/network/http/http_client.h>
#include <nx/network/test_support/buffer_socket.h>


static const qint64 MAX_BYTES_TO_READ = 25*1024*1024;

//static const QString rtspUrl( QLatin1String("rtsp://192.168.0.27:554/axis-media/media.amp?streamprofile=hdwitnessSecondary") );
static const QString rtspUrl( QLatin1String("rtsp://192.168.0.27:554/axis-media/media.amp?streamprofile=hdwitnessPrimary") );

//static const QString rtspUrl( QLatin1String("rtsp://192.168.0.27/axis-media/media.amp") );


TEST( QnMulticodecRtpReader, DISABLED_streamFetchingOverRTSP )
{
    //QnResourcePropertyDictionary resPropertyDictionary;
    QnSyncTime syncTimeInstance;

    QnNetworkResourcePtr resource( new QnNetworkResource() );
    resource->setId( QUuid::createUuid() );
    resource->setName( "DummyRes" );
    QAuthenticator auth;
    auth.setUser( "root" );
    auth.setPassword( "ptth" );
    resource->setAuth( auth );

    std::unique_ptr<QnMulticodecRtpReader> rtspStreamReader( new QnMulticodecRtpReader( resource ) );
    rtspStreamReader->setRequest( rtspUrl );
    ASSERT_TRUE( static_cast<bool>(rtspStreamReader->openStream()) );

    int frameCount = 0;
    QElapsedTimer t;
    t.restart();
    qint64 bytesReadPerInterval = 0;
    qint64 totalBytesRead = 0;
    for( ; totalBytesRead< MAX_BYTES_TO_READ; )
    {
        if( t.elapsed() > 10*1000 )
        {
            std::cout<<
                "fps "<<std::setprecision(2)<<(frameCount / (t.elapsed() / 1000.0))<<
                ", bitrate "<<(bytesReadPerInterval * CHAR_BIT * 1000 / t.elapsed() / 1024)<<"Kbps"<<
                ", total bytes read "<<totalBytesRead<<
                std::endl;
            frameCount = 0;
            bytesReadPerInterval = 0;
            t.restart();
        }
        auto frame = rtspStreamReader->getNextData();
        if( !frame )
        {
            std::cerr<<"Failed to get frame from "<<rtspUrl.toStdString()<<std::endl;
            continue;
        }
        bytesReadPerInterval += frame->dataSize();
        totalBytesRead += frame->dataSize();
        ++frameCount;
    }

    std::cout<<
        "fps "<<std::setprecision(2)<<(frameCount / (t.elapsed() / 1000.0))<<
        ", bitrate "<<(bytesReadPerInterval * CHAR_BIT * 1000 / t.elapsed() / 1024)<<"Kbps"<<
        ", total bytes read "<<totalBytesRead<<
        std::endl;
}

TEST( QnMulticodecRtpReader, DISABLED_streamFetchingOverRTSP2 )
{
    QAuthenticator auth;
    auth.setUser( "root" );
    auth.setPassword( "ptth" );
    QnRtspClient rtspClient(true);
    rtspClient.setAuth( auth, nx_http::header::AuthScheme::basic );

    rtspClient.setTransport( "tcp" );
    ASSERT_TRUE( rtspClient.open( rtspUrl ).errorCode == 0 );
    rtspClient.play( AV_NOPTS_VALUE, AV_NOPTS_VALUE, 1.0 );

    quint8 buf[4*1024];
    qint64 totalBytesRead = 0;
    size_t readsCount = 0;
    for( ; static_cast<size_t>(totalBytesRead) < MAX_BYTES_TO_READ; )
    {
        int bytesRead = rtspClient.readBinaryResponce( buf, sizeof(buf) );
        if( bytesRead <= 0 )
            break;
        totalBytesRead += bytesRead;
        ++readsCount;
    }

    std::cout<<"totalBytesRead "<<totalBytesRead<<", average read size "<<(totalBytesRead/readsCount)<<" bytes"<<std::endl;
}

//#define USE_POLL

TEST( QnMulticodecRtpReader, DISABLED_streamFetchingOverRTSP3 )
{
    QAuthenticator auth;
    auth.setUser( "root" );
    auth.setPassword( "ptth" );
    QnRtspClient rtspClient(true);
    rtspClient.setAuth( auth, nx_http::header::AuthScheme::basic );

    rtspClient.setTransport( "tcp" );
    ASSERT_TRUE( rtspClient.open( rtspUrl ).errorCode == 0 );
    rtspClient.play( AV_NOPTS_VALUE, AV_NOPTS_VALUE, 1.0 );

    quint8 buf[64*1024];
    memset( buf, 'x', sizeof(buf) );
    qint64 totalBytesRead = 0;
    size_t readsCount = 0;

#ifdef USE_POLL
    struct pollfd pollset[1];
    memset( pollset, 0, sizeof(pollset) );
    pollset[0].fd = rtspClient.tcpSock()->handle();
    pollset[0].events |= POLLIN;
#endif

    for( ; static_cast<size_t>(totalBytesRead) < MAX_BYTES_TO_READ; )
    {
#ifdef USE_POLL
        pollset[0].revents = 0;
        int ret = poll( pollset, sizeof(pollset) / sizeof(*pollset), -1 );
        if( ret == 0 )
            continue;

        if( (pollset[0].revents & POLLIN) == 0 )
            break;  //error occured
#endif
        //QThread::msleep( 5 );

        int bytesRead = 0;
        bytesRead = rtspClient.tcpSock()->recv(
            buf,
            sizeof(buf),
#ifdef USE_POLL
            MSG_DONTWAIT
#else
            0
#endif
            );

        if( bytesRead <= 0 )
        {
            if( errno == EAGAIN || errno == EWOULDBLOCK  )
                continue;
            std::cerr<<"Failed to read from socket. "<<strerror(errno)<<std::endl;
            break;
        }

        totalBytesRead += bytesRead;
        ++readsCount;
    }

    std::cout<<"totalBytesRead "<<totalBytesRead<<", average read size "<<(totalBytesRead/readsCount)<<" bytes"<<std::endl;
}

TEST( QnMulticodecRtpReader, DISABLED_streamFetchingOverHTTP )
{
    nx_http::HttpClient client;
    ASSERT_TRUE( client.doGet( nx::utils::Url("http://192.168.0.1/valgrind-arm-linaro-multilib-2013.09.tar.gz") ) );
    qint64 totalBytesRead = 0;
    size_t readsCount = 0;
    while( !client.eof() && (static_cast<size_t>(totalBytesRead) < MAX_BYTES_TO_READ) )
    {
        const auto& buf = client.fetchMessageBodyBuffer();
        totalBytesRead += buf.size();
        ++readsCount;
    }

    std::cout<<"totalBytesRead "<<totalBytesRead<<", average read size "<<(totalBytesRead/readsCount)<<" bytes"<<std::endl;
}

TEST( QnMulticodecRtpReader, DISABLED_streamFetchingOverHTTP2 )
{
    std::unique_ptr<AbstractStreamSocket> sock( SocketFactory::createStreamSocket() );
    ASSERT_TRUE( sock->connect( SocketAddress("192.168.0.1", 80) ) );
    sock->send( QByteArray("GET /valgrind-arm-linaro-multilib-2013.09.tar.gz HTTP/1.0\r\n\r\n") );
    char buf[4*1024];
    qint64 totalBytesRead = 0;
    size_t readsCount = 0;
    for( ; static_cast<size_t>(totalBytesRead) < MAX_BYTES_TO_READ; )
    {
        //QThread::msleep( 5 );

        int bytesRead = sock->recv( buf, sizeof(buf), 0 );
        if( bytesRead <= 0 )
            break;
        totalBytesRead += bytesRead;
        ++readsCount;
    }

    std::cout<<"totalBytesRead "<<totalBytesRead<<", average read size "<<(totalBytesRead/readsCount)<<" bytes"<<std::endl;
}


#ifdef __linux__
#define USE_SPLICE
#endif

TEST( QnMulticodecRtpReader, DISABLED_streamFetchingOverRTSP5 )
{
    QAuthenticator auth;
    auth.setUser( "root" );
    auth.setPassword( "ptth" );
    QnRtspClient rtspClient(true);
    rtspClient.setAuth( auth, nx_http::header::AuthScheme::basic );

    rtspClient.setTransport( "tcp" );
    ASSERT_TRUE( rtspClient.open( rtspUrl ).errorCode == 0 );
    rtspClient.play( AV_NOPTS_VALUE, AV_NOPTS_VALUE, 1.0 );

    quint8 buf[4*1024];
    memset( buf, 'x', sizeof(buf) );
    qint64 totalBytesRead = 0;
    size_t readsCount = 0;

#ifdef USE_SPLICE
    int pipefd[2];
    memset( pipefd, 0, sizeof(pipefd) );
    if( pipe2( pipefd, O_NONBLOCK ) != 0 )
    {
        std::cerr<<"Failed to create pipe. "<<strerror(errno)<<std::endl;
        return;
    }

    //struct iovec iov;
    //iov.iov_base = buf;
    //iov.iov_len = sizeof(buf);
    //const int pipeBufSize = vmsplice( pipefd[1], &iov, 1, 0 );
    //if( pipeBufSize == -1 )
    //{
    //    std::cerr<<"Failed vmsplice. "<<strerror(errno)<<std::endl;
    //    return;
    //}
    //std::cout<<"pipeBufSize = "<<pipeBufSize<<std::endl;
#endif

    struct pollfd pollset[1];
    memset( pollset, 0, sizeof(pollset) );
    pollset[0].fd = rtspClient.tcpSock()->handle();
    pollset[0].events |= POLLIN;

    for( ; static_cast<size_t>(totalBytesRead) < MAX_BYTES_TO_READ; )
    {
        ////QThread::msleep( 5 );
        //pollset[0].revents = 0;
        //int ret = poll( pollset, sizeof(pollset) / sizeof(*pollset), -1 );
        //if( ret == 0 )
        //    continue;

        //if( (pollset[0].revents & POLLIN) == 0 )
        //    break;  //error occured

        int bytesRead = 0;
#ifdef USE_SPLICE
        //std::cout<<"before splice"<<std::endl;
        bytesRead = splice(
            rtspClient.tcpSock()->handle(),
            NULL,
            pipefd[1],
            NULL,
            sizeof(buf),
            SPLICE_F_MOVE | SPLICE_F_NONBLOCK );
        //std::cout<<"splice returned "<<bytesRead<<std::endl;

        //std::cout<<std::string( (const char*)buf, bytesRead )<<std::endl;

        bytesRead = read( pipefd[0], buf, bytesRead );
        //std::cout<<"splice returned "<<bytesRead<<", bytesRead2 "<<bytesRead2<<std::endl;
#else
        bytesRead = rtspClient.tcpSock()->recv( buf, sizeof(buf), 0 );
#endif

        if( bytesRead <= 0 )
        {
            if( errno == EAGAIN || errno == EWOULDBLOCK  )
                continue;
            std::cerr<<"Failed to read from socket. "<<strerror(errno)<<std::endl;
            break;
        }

        //bytesRead = rtspClient.tcpSock()->recv( buf, sizeof(buf), /*MSG_WAITALL*/ MSG_DONTWAIT );
        //std::cout<<"recv returned "<<bytesRead<<std::endl;
        //if( bytesRead <= 0 )
        //    std::cerr<<strerror(errno)<<std::endl;

        totalBytesRead += bytesRead;
        ++readsCount;
    }

    std::cout<<"totalBytesRead "<<totalBytesRead<<", average read size "<<(totalBytesRead/readsCount)<<" bytes"<<std::endl;
}


namespace
{
    class AsyncReadHandler
    {
    public:
        qint64 totalBytesRead;
        size_t readsCount;

        AsyncReadHandler( AbstractCommunicatingSocket* sock )
        :
            totalBytesRead( 0 ),
            readsCount( 0 ),
            m_sock( sock ),
            m_done( false )
        {
            m_buf.reserve( 4*1024 );
        }

        void onBytesRead( SystemError::ErrorCode errorCode, size_t bytesRead )
        {
            if( errorCode == SystemError::noError )
            {
                totalBytesRead += bytesRead;
                ++readsCount;
            }

            if( (errorCode != SystemError::noError) ||
                (bytesRead == 0) ||
                (static_cast<size_t>(totalBytesRead) > MAX_BYTES_TO_READ) )
            {
                //std::unique_lock<std::mutex> lk( m_mutex );
                m_done = true;
                //m_cond.notify_all();
                return;
            }

            m_buf.resize(0);

            using namespace std::placeholders;
            m_sock->readSomeAsync(
                &m_buf,
                std::bind( &AsyncReadHandler::onBytesRead, this, _1, _2 ) );
        }

        void start()
        {
            using namespace std::placeholders;
            m_sock->readSomeAsync(
                &m_buf,
                std::bind( &AsyncReadHandler::onBytesRead, this, _1, _2 ) );
        }

        void wait()
        {
            while( !m_done )
            {
                QThread::sleep( 1 );
            }

            ////waiting
            //std::unique_lock<std::mutex> lk( m_mutex );
            //m_cond.wait( lk, [this](){ return m_done; } );
        }

        bool done() const
        {
            return m_done;
        }

    private:
        AbstractCommunicatingSocket* m_sock;
        nx::Buffer m_buf;
        std::mutex m_mutex;
        std::condition_variable m_cond;
        bool m_done;
    };
}

static const int SESSION_COUNT = 2;

TEST( QnMulticodecRtpReader, DISABLED_streamFetchingOverRTSP7 )
{
    QAuthenticator auth;
    auth.setUser( "root" );
    auth.setPassword( "ptth" );

    std::vector<std::unique_ptr<QnRtspClient>> RtspClients;
    RtspClients.resize( SESSION_COUNT );
    for( auto& rtspClient: RtspClients )
    {
        rtspClient.reset( new QnRtspClient(true) );
        rtspClient->setAuth( auth, nx_http::header::AuthScheme::basic );
        rtspClient->setTransport( "tcp" );
        ASSERT_TRUE( rtspClient->open( rtspUrl ).errorCode == 0 );
        rtspClient->play( AV_NOPTS_VALUE, AV_NOPTS_VALUE, 1.0 );
    }

    std::vector<std::unique_ptr<AsyncReadHandler>> asyncReadHandlers;
    asyncReadHandlers.resize( SESSION_COUNT );
    for( size_t i = 0; i < asyncReadHandlers.size(); ++i )
    {
        asyncReadHandlers[i].reset( new AsyncReadHandler( RtspClients[i]->tcpSock() ) );
        asyncReadHandlers[i]->start();
    }

    for( ;; )
    {
        QThread::sleep( 1 );
        bool done = true;
        for( auto& asyncReadHandler: asyncReadHandlers )
            done = done & asyncReadHandler->done();
        if( done )
            break;
    }

    size_t totalBytesRead = 0;
    size_t readsCount = 0;
    for( auto& asyncReadHandler: asyncReadHandlers )
    {
        totalBytesRead += asyncReadHandler->totalBytesRead;
        readsCount += asyncReadHandler->readsCount;
    }

    std::cout<<
        "totalBytesRead "<<totalBytesRead<<", "
        "average read size "<<(totalBytesRead / readsCount)<<" bytes"<<std::endl;
}

TEST( QnMulticodecRtpReader, DISABLED_streamFetchingOverRTSP8 )
{
    QAuthenticator auth;
    auth.setUser( "root" );
    auth.setPassword( "ptth" );

    std::vector<std::unique_ptr<QnRtspClient>> RtspClients;
    RtspClients.resize( SESSION_COUNT );
    for( auto& rtspClient: RtspClients )
    {
        rtspClient.reset( new QnRtspClient(true) );
        rtspClient->setAuth( auth, nx_http::header::AuthScheme::basic );
        rtspClient->setTransport( "tcp" );
        ASSERT_TRUE( rtspClient->open( rtspUrl ).errorCode == 0 );
        rtspClient->play( AV_NOPTS_VALUE, AV_NOPTS_VALUE, 1.0 );
    }

    quint8 buf[64*1024];
    memset( buf, 'x', sizeof(buf) );
    qint64 totalBytesRead = 0;
    size_t readsCount = 0;

    unsigned int x = 0;
    for( ; totalBytesRead < MAX_BYTES_TO_READ * SESSION_COUNT; )
    {
        int bytesRead = 0;
        bytesRead = RtspClients[x % SESSION_COUNT]->tcpSock()->recv(
            buf,
            sizeof(buf),
            0 );

        if( bytesRead <= 0 )
        {
            if( errno == EAGAIN || errno == EWOULDBLOCK  )
                continue;
            std::cerr<<"Failed to read from socket. "<<strerror(errno)<<std::endl;
            break;
        }

        totalBytesRead += bytesRead;
        ++readsCount;
        ++x;
    }

    std::cout<<"totalBytesRead "<<totalBytesRead<<", average read size "<<(totalBytesRead/readsCount)<<" bytes"<<std::endl;
}

TEST( QnMulticodecRtpReader, DISABLED_rtpParsingPerformance )
{
    //creating necessary single-tones
    //QnResourcePropertyDictionary resPropertyDictionary;
    QnSyncTime syncTimeInstance;

    //preparing test data
    //static const char* testDataFilePath = "D:\\develop\\problems\\bpi_optimization\\in.1";
    static const char* testDataFilePath = "/home/pi/in.1";
    std::ifstream testFile;
    testFile.open( testDataFilePath, std::ifstream::binary );
    std::string testData;
    struct stat st;
    memset( &st, 0, sizeof(st) );
    ASSERT_EQ( stat( testDataFilePath, &st ), 0 );
    testData.resize( st.st_size );
    testFile.read( const_cast<char*>(testData.data()), testData.size() );
    ASSERT_EQ( (size_t)testFile.gcount(), testData.size() );
    testFile.close();

    QnNetworkResourcePtr resource( new QnNetworkResource() );
    resource->setId( QUuid::createUuid() );
    resource->setName( "DummyRes" );
    QAuthenticator auth;
    auth.setUser( "root" );
    auth.setPassword( "ptth" );
    resource->setAuth( auth );

    QElapsedTimer t;
    t.restart();
    for( int i = 0; i < 1; ++i )
    {
        std::unique_ptr<QnMulticodecRtpReader> rtspStreamReader(
            new QnMulticodecRtpReader(
                resource,
                std::make_unique<nx::network::BufferSocket>(testData)) );
        rtspStreamReader->setRequest( rtspUrl );
        ASSERT_TRUE( static_cast<bool>(rtspStreamReader->openStream()) );

        for( ;; )
        {
            auto frame = rtspStreamReader->getNextData();
            if( !frame )
                break;
        }
    }

    int x = t.elapsed();
    std::cout<<"Done in "<<x<<"ms"<<std::endl;
}

#endif
