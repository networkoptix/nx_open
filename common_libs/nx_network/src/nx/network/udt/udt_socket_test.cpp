#include "udt_socket_test.h"
#include <thread>
#include <mutex>
#include <queue>
#include <array>
#include <functional>
#include <cstdint>
#include <random>
#include <ctime>
#include <iostream>
#include <atomic>
#include <QCoreApplication>
#include <QCommandLineParser>

#include <nx/network/udt/udt_socket.h>
#include <nx/network/socket_factory.h>

namespace {
// Configuration or client/server
struct ServerConfig {
    QString address;
    int port;
    int tcp;
};

struct ClientConfig {
    QString address;
    int port;
    // statistics for simulation
    float active_loader_rate;
    float disconnect_rate;
    std::uint32_t maximum_connection_size;
    int content_min_size;
    int content_max_size;
    int conn_size_lower;
    int conn_size_upper;
    int conn_time_lower;
    int conn_time_upper;
    int tcp;
    int reused_port;
};

template< typename T >
class Queue {
public:
    void Enqueue( const T& val ) {
        std::list<T> element(1,val);
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.splice(queue_.end(),std::move(element));
    }
    void Enqueue( T&& val ) {
        std::list<T> element(1,std::move(val));
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.splice(queue_.end(),std::move(element));
    }
    bool Dequeue( T* val ) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(queue_.empty()) return false;
        *val = queue_.front();
        // C++ standard says delete will _NEVER_ throw exception
        queue_.pop_front();
        return true;
    }
private:
    std::mutex mutex_;
    std::list<T> queue_;
};


class Ticker {
public:
    Ticker() : prev_time_(-1), timeout_(0) , next_timeout_(0) {}
    void ScheduleNextTick( 
        std::function<void()>&& callback , int timeout ) {
            next_timeout_ = timeout;
            callback_ = std::move(callback);
    }
    void Update() {
        if( prev_time_ == -1 ) {
            if( callback_ )
                callback_();
            prev_time_ = std::clock();
        } else {
            int cur_time = std::clock();
            timeout_ += cur_time - prev_time_;
            if( timeout_ > next_timeout_ ) {
                timeout_ -= next_timeout_;
                if( callback_ )
                    callback_();
            }
            prev_time_ = cur_time;
        }
    }
private:
    int prev_time_;
    int timeout_;
    int next_timeout_;
    std::function<void()> callback_;
};

}// namespace 

// ====================================================
// Profile Server Implementation
// ====================================================

// The test server will simply do 2 things:
// 1. Hang on read from the socket
// 2. Allow more acceptance of the socket
// We have no write currently for test server implementation

class UdtSocketProfileServer : public UdtSocketProfile {
public:
    UdtSocketProfileServer( const ServerConfig& config ) :
        server_socket_(SocketFactory::createStreamServerSocket(
            false, config.tcp ? SocketFactory::NatTraversalType::nttDisabled
                              : SocketFactory::NatTraversalType::nttEnabled)),
        address_(config.address),
        port_(config.port),
        current_conn_size_(0),
        total_recv_bytes_(0),
        broken_or_error_conn_(0),
        closed_conn_(0)
    {
    }
    virtual bool run() {
        if(!server_socket_->bind( SocketAddress(HostAddress(address_),port_)))
            return false;
        if(!server_socket_->listen()) 
            return false;
        server_socket_->acceptAsync(
            std::bind(
            &UdtSocketProfileServer::OnAccept,
            this,
            std::placeholders::_1,
            std::placeholders::_2 ));
        // Print the shit here
        while( true ) {
            std::cout<<"=========================================\n";
            std::cout<<"Connected socket:"<<current_conn_size_<<"\n";
            std::cout<<"Broken or error socket:"<<broken_or_error_conn_<<"\n";
            std::cout<<"Closed connection:"<<closed_conn_<<"\n";
            std::cout<<"Total received bytes:"<<total_recv_bytes_<<"\n";
            std::cout<<"========================================="<<std::endl;
            std::this_thread::sleep_for(
                std::chrono::milliseconds(1000));
        }
        return true;
    }
    virtual void quit() {
        server_socket_->cancelAsyncIO(true);
    }
private:
    struct Connection {
        UdtStreamSocket* socket;
        nx::Buffer buffer;
        Connection() {
            buffer.reserve(1024);
        }
        ~Connection() {
            delete socket;
        }
    };
    void OnAccept( SystemError::ErrorCode error_code , AbstractStreamSocket* new_conn ) {
        if( !error_code ) {
            std::unique_ptr<Connection> conn( new Connection );
            conn->socket = static_cast<UdtStreamSocket*>(new_conn);
            new_conn->readSomeAsync(
                &(conn->buffer),
                std::bind(
                &UdtSocketProfileServer::OnRead,
                this,
                std::placeholders::_1,
                std::placeholders::_2,
                conn.get()));
            ++current_conn_size_;
            conn.release();
        }
        server_socket_->acceptAsync(
            std::bind(
            &UdtSocketProfileServer::OnAccept,
            this,
            std::placeholders::_1,
            std::placeholders::_2 ));
    }
    void OnRead( SystemError::ErrorCode error_code , std::size_t bytes_transferred , Connection* ptr ) {
        std::unique_ptr<Connection> conn(ptr);
        if(error_code) {
            conn->socket->cancelAsyncIO(aio::etNone );
            conn->socket->close();
            --current_conn_size_;
            ++broken_or_error_conn_;
        } else {
            if( bytes_transferred == 0 ) {
                ++closed_conn_;
                --current_conn_size_;
                conn->socket->cancelAsyncIO(aio::etNone );
                conn->socket->close();
            } else {
                total_recv_bytes_ += bytes_transferred;
                conn->buffer.resize(0);
                conn->socket->readSomeAsync(
                    &(conn->buffer),
                    std::bind(
                    &UdtSocketProfileServer::OnRead,
                    this,
                    std::placeholders::_1,
                    std::placeholders::_2,
                    conn.get()));
                conn.release();
            }
        }
    }
private:
    static const int kMaximumSleepTime = 400;
    static const std::uint32_t kPrintStatisticFrequency = 1000;
    // Server socket
    std::unique_ptr<AbstractStreamServerSocket> server_socket_;
    // Address
    QString address_;
    int port_;
    // Statistic 
    int current_conn_size_;
    std::size_t total_recv_bytes_;
    std::size_t broken_or_error_conn_;
    std::size_t closed_conn_;
};

namespace {

class RandomHelper {
public:
    // Call this function to roll a dice. If the probability hits,
    // the function returns true, otherwise it return false.
    bool Roll( float probability ) {
        Q_ASSERT( probability >= 0.0f && probability <= 1.0f );
        std::uniform_real_distribution<> distr(0.0,1.0);
        if( distr(engine_) > probability ) return false;
        return true;
    }
    // Generate Random integer
    int RandomRange( int min , int max ) {
        Q_ASSERT( min <= max );
        std::uniform_int_distribution<int> distr(min,max);
        return distr(engine_);
    }
    // Generate Random string
    nx::Buffer RandomString( int length ) {
        nx::Buffer ret;
        ret.reserve(length);
        std::uniform_int_distribution<char> distr(std::numeric_limits<char>::min(),std::numeric_limits<char>::max());
        for( std::size_t i = 0 ; i < static_cast<std::size_t>(length) ; ++i ) {
            ret.push_back( distr(engine_) );
        }
        return ret;
    }
    RandomHelper() {
        engine_.seed( std::time(0)  );
    }
private:
    std::mt19937 engine_;
};

}// namespace

class UdtSocketProfileClient : public UdtSocketProfile {
public:
    UdtSocketProfileClient( const ClientConfig& config ) :
        address_(config.address),
        port_(config.port),
        reused_port_(config.reused_port),
        tcp_(config.tcp),
        active_load_factor_(config.active_loader_rate),
        disconnect_rate_(config.disconnect_rate),
        maximum_allowed_content_(config.content_max_size),
        minimum_allowed_content_(config.content_min_size),
        maximum_allowed_concurrent_connection_(config.maximum_connection_size),
        connected_socket_size_(0),
        active_conn_sockets_size_(0),
        failed_connection_size_(0),
        closed_conn_socket_size_(0),
        conn_size_lower_(config.conn_size_lower),
        conn_size_upper_(config.conn_size_upper),
        conn_time_lower_(config.conn_time_lower),
        conn_time_upper_(config.conn_time_upper),
        quit_(false) { }

    virtual bool run() {
        conn_ticker_.ScheduleNextTick( std::bind(&UdtSocketProfileClient::ScheduleConnection,this),0);
        while( !quit_ ) {
            ScheduleSleepSockets();
            PrintStatistic();
            std::this_thread::sleep_for(
                std::chrono::milliseconds(1000));
            conn_ticker_.Update();
        }
        return true;
    }
    virtual void quit() {
        quit_ = true;
    }
private:
    struct Connection {
        Connection( AbstractStreamSocket* s ) :
            socket(s) {}
        std::unique_ptr<AbstractStreamSocket> socket;
        nx::Buffer buffer;
    };
    void OnConnect( SystemError::ErrorCode error_code , Connection* ptr ) {
        std::unique_ptr<Connection> conn(ptr);
        if(!error_code) {
            ++connected_socket_size_;
            sleep_list_.Enqueue( conn.release() );
        }
        --active_conn_sockets_size_;
    }
    void OnWrite( SystemError::ErrorCode error_code , std::size_t bytes_transferred , Connection* ptr ) {
        std::unique_ptr<Connection> conn(ptr);
        if( error_code ) {
            conn->socket->cancelAsyncIO(aio::EventType::etWrite);
            conn->socket->close();
            failed_connection_size_++;
            --connected_socket_size_;
        } else {
            Q_ASSERT( bytes_transferred == conn->buffer.size() );
            sleep_list_.Enqueue( conn.release() );
        }
    }
private:

    void PrintStatistic() {
        std::cout<<"====================================================\n";
        std::cout<<"Active connection:"<<connected_socket_size_<<"\n";
        std::cout<<"Closed connection:"<<closed_conn_socket_size_<<"\n";
        std::cout<<"Failed connection:"<<failed_connection_size_<<"\n";
        std::cout<<"===================================================="<<std::endl;
    }
    void ScheduleWrite( Connection* ptr ) {
        // We can write data to the user
        std::unique_ptr<Connection> conn(ptr);

        int length = random_.RandomRange(minimum_allowed_content_,maximum_allowed_content_);
        conn->buffer = random_.RandomString(length);

        conn->socket->sendAsync(
            conn->buffer,
            std::bind(
            &UdtSocketProfileClient::OnWrite,
            this,
            std::placeholders::_1,std::placeholders::_2,conn.get()));
        conn.release();
    }
    void ScheduleSleepSockets();
    bool ScheduleSleepSocket( Connection* ptr ) {
        std::unique_ptr<Connection> conn(ptr);
        // Because UDT has bug related to detect closed socket in epoll API
        // so we don't issue any Clean Close in remote peer and no test for
        if( random_.Roll(active_load_factor_) ) {
            // Schedule this sleepy socket
            ScheduleWrite( conn.release() );
            return true;
        } else if( random_.Roll(disconnect_rate_) ) {
            conn->socket->cancelAsyncIO();
            conn->socket->close();
            --connected_socket_size_;
            ++closed_conn_socket_size_;
            return true;
        }
        conn.release();
        return false;
    }
    void ScheduleConnection();
    float GetConnectionProbability() const {
        const int half_max = maximum_allowed_concurrent_connection_/2;
        if( connected_socket_size_ < half_max )
            return 1.0f;
        else {
            int left = maximum_allowed_concurrent_connection_ - connected_socket_size_;
            Q_ASSERT( left>=0 );
            const float lower_bound = 0.7f;
            return lower_bound + (left/half_max)*(1.0f-lower_bound);
        }
    }
private:
    static const int kMaximumSleepTime = 200;
    static const std::uint32_t kPrintStatisticFrequency = 1000;
    std::size_t thread_size_;
    QString address_;
    int port_;
    int reused_port_;
    bool tcp_;
    RandomHelper random_;
    // statistic configuration
    float active_load_factor_;
    float disconnect_rate_;
    int maximum_allowed_content_;
    int minimum_allowed_content_;
    int maximum_allowed_concurrent_connection_;
    int connected_socket_size_;
    int active_conn_sockets_size_;
    int failed_connection_size_;
    int closed_conn_socket_size_;

    int conn_size_lower_;
    int conn_size_upper_;
    int conn_time_lower_;
    int conn_time_upper_;

    Ticker conn_ticker_;
    // the list that cache all the inactive sockets
    Queue<Connection*> sleep_list_;
    bool quit_;
};

void UdtSocketProfileClient::ScheduleSleepSockets() {
    static const std::size_t kMaximumIterationSize = 128;
    std::size_t iterate_size = kMaximumIterationSize;
    while( true ) {
        Connection* conn;
        if( !sleep_list_.Dequeue(&conn) )
            break;
        // Handle this conn
        if(!ScheduleSleepSocket(conn)) {
            sleep_list_.Enqueue(conn);
        }
        --iterate_size;
        if( iterate_size == 0 )
            break;
    }
}

void UdtSocketProfileClient::ScheduleConnection() {
    // Schedule the connection
    std::size_t per_conn_size = static_cast<std::size_t>(random_.RandomRange( conn_size_lower_ , conn_size_upper_ ));
    std::size_t scheduled_size = 0;
    for( std::size_t i = active_conn_sockets_size_ ; (i < maximum_allowed_concurrent_connection_) && 
                                               (connected_socket_size_ < maximum_allowed_concurrent_connection_) ; ++i ) {
        if( random_.Roll( GetConnectionProbability() ) ) {
            SocketFactory::NatTraversalType type = tcp_ ? SocketFactory::NatTraversalType::nttDisabled : SocketFactory::NatTraversalType::nttEnabled;
            std::unique_ptr<Connection> conn(  new Connection( SocketFactory::createStreamSocket( false , type ).release() ) );
            conn->socket->setNonBlockingMode(true);
            conn->socket->setReuseAddrFlag(true);
            if( !conn->socket->bind(SocketAddress(HostAddress::anyHost,reused_port_)) )
                continue;
            conn->socket->connectAsync(
                SocketAddress( HostAddress(address_), port_ ) ,
                std::bind(
                &UdtSocketProfileClient::OnConnect,
                this,
                std::placeholders::_1, conn.get()));
            conn.release();
            ++active_conn_sockets_size_;
            ++scheduled_size;
            if( scheduled_size > per_conn_size ) {
                break;
            }
        }
    }
    conn_ticker_.ScheduleNextTick( std::bind(&UdtSocketProfileClient::ScheduleConnection,this),
        random_.RandomRange(conn_time_lower_,conn_time_upper_));
}

// The command line parameters
namespace {

class CommandLineParser {
public:
    CommandLineParser( const QCoreApplication& app ) :
        application_(app){}
    bool ParseCommandLine();
    bool IsServer() const {
        return parser_.isSet(QLatin1String("s"));
    }
    bool ParseServerConfig( ServerConfig* config );
    bool ParseClientConfig( ClientConfig* config );
private:
    const QCoreApplication& application_;
    QCommandLineParser parser_;
};

bool CommandLineParser::ParseCommandLine() {
    // Adding option for the command line now
    parser_.setApplicationDescription(QLatin1String("UDT performance profile program"));
    parser_.addHelpOption();
    parser_.addOption( QCommandLineOption(QLatin1String("s"),
        QLatin1String("Indicate whether it is a server or a client")) );
    parser_.addOption( QCommandLineOption(QLatin1String("tcp"),
        QLatin1String("Indicate whether to use tcp or udt")) );
    parser_.addOption(
        QCommandLineOption(QLatin1String("addr"),QLatin1String("IPV4 address"),QLatin1String("addr")));
    parser_.addOption(
        QCommandLineOption(QLatin1String("port"),QLatin1String("Port number"),QLatin1String("port")));
    parser_.addOption(
        QCommandLineOption(QLatin1String("reused_port"),QLatin1String("Reused port number"),QLatin1String("reused_port")));
    parser_.addOption(
        QCommandLineOption(QLatin1String("active_rate"),QLatin1String("Active client rate,range in [0.0,1.0],if it is a client."),QLatin1String("active_rate")));
    parser_.addOption(
        QCommandLineOption(QLatin1String("disconn_rate"),QLatin1String("Disconnection rate,range in [0.0,1.0],if it is a client."),QLatin1String("disconn_rate")));
    parser_.addOption(
        QCommandLineOption(QLatin1String("max_conn"),QLatin1String("Maximum concurrent connection,if it is a client."),QLatin1String("max_conn")));
    parser_.addOption(
        QCommandLineOption(QLatin1String("min_content_len"),
        QLatin1String("Minimum content length for random content generation,if it is a client and it is optional."),
        QLatin1String("min_content_len")));
    parser_.addOption(
        QCommandLineOption(QLatin1String("max_content_len"),
        QLatin1String("Maximum content length for random content generation,if it is a client and it is optional."),
        QLatin1String("max_content_len")));
    parser_.addOption(
        QCommandLineOption(QLatin1String("conn_size_lower"),
        QLatin1String("Minimum connection size per schedule."),
        QLatin1String("conn_size_lower")));
    parser_.addOption(
        QCommandLineOption(QLatin1String("conn_size_upper"),
        QLatin1String("Maximum connection size per schedule."),
        QLatin1String("conn_size_upper")));
    parser_.addOption(
        QCommandLineOption(QLatin1String("conn_time_lower"),
        QLatin1String("Minimum connection size per schedule."),
        QLatin1String("conn_time_lower")));
    parser_.addOption(
        QCommandLineOption(QLatin1String("conn_time_upper"),
        QLatin1String("Minimum connection size per schedule."),
        QLatin1String("conn_time_upper")));
    parser_.process(application_);
    return true;
}

bool CommandLineParser::ParseServerConfig( ServerConfig* config ) {
    Q_ASSERT(IsServer());
    if( !parser_.isSet(QLatin1String("addr")) || 
        !parser_.isSet(QLatin1String("port")) ) {
            std::cout<<parser_.helpText().toLatin1().constData()<<std::endl;
            return false;
    } else {
        config->address = parser_.value(QLatin1String("addr"));
        config->port = parser_.value(QLatin1String("port")).toInt();
        if( config->port == 0 ) {
            std::cout<<parser_.helpText().toLatin1().constData()<<std::endl;
            return false;
        }
        if( parser_.isSet(QLatin1String("tcp")) ) {
            config->tcp = true;
        } else {
            config->tcp = false;
        }
        return true;
    }
}

float Clamp( float c ) {
    if( c > 1.0f )
        return 1.0f;
    else if( c < 0.0f )
        return 0.0f;
    else 
        return c;
}

bool CommandLineParser::ParseClientConfig( ClientConfig* config ) {
    if( !parser_.isSet(QLatin1String("addr")) ||
        !parser_.isSet(QLatin1String("port")) ||
        !parser_.isSet(QLatin1String("active_rate")) ||
        !parser_.isSet(QLatin1String("disconn_rate")) ||
        !parser_.isSet(QLatin1String("max_conn")) ||
        !parser_.isSet(QLatin1String("conn_size_lower")) ||
        !parser_.isSet(QLatin1String("conn_size_upper")) ||
        !parser_.isSet(QLatin1String("conn_time_lower")) ||
        !parser_.isSet(QLatin1String("conn_time_upper")) ){
            std::cout<<parser_.helpText().toLatin1().constData()<<std::endl;
            return false;
    } else {
        config->address = parser_.value(QLatin1String("addr"));
        config->port = parser_.value(QLatin1String("port")).toInt();
        if( config->port == 0 ) {
            std::cout<<parser_.helpText().toLatin1().constData()<<std::endl;
            return false;
        }
        if( parser_.isSet(QLatin1String("reused_port")) ) {
            config->reused_port = parser_.value(QLatin1String("reused_port")).toInt();
        } else {
            config->reused_port = 0;
        }
        config->active_loader_rate = parser_.value(QLatin1String("active_rate")).toFloat();
        config->active_loader_rate = Clamp(config->active_loader_rate);
        config->disconnect_rate = parser_.value(QLatin1String("disconn_rate")).toFloat();
        config->disconnect_rate = Clamp( config->disconnect_rate );
        config->maximum_connection_size = parser_.value(QLatin1String("max_conn")).toUInt();
        if( parser_.isSet(QLatin1String("min_content_len")) )
            config->content_min_size = parser_.value(QLatin1String("min_content_len")).toInt();
        else
            config->content_min_size = 1024;
        if( parser_.isSet(QLatin1String("max_content_len")) ) 
            config->content_max_size = parser_.value(QLatin1String("max_content_len")).toInt();
        else
            config->content_max_size = 4096;
        config->conn_size_lower = parser_.value(QLatin1String("conn_size_lower")).toInt();
        config->conn_size_upper = parser_.value(QLatin1String("conn_size_upper")).toInt();
        config->conn_time_lower = parser_.value(QLatin1String("conn_time_lower")).toInt();
        config->conn_time_upper = parser_.value(QLatin1String("conn_time_upper")).toInt();
        if( parser_.isSet(QLatin1String("tcp")) ) {
            config->tcp = true;
        } else {
            config->tcp = false;
        }
        return true;
    }
}

}// namespace

std::unique_ptr<UdtSocketProfile> createUdtSocketProfile( const QCoreApplication& app ) {
    CommandLineParser parser(app);
    if(!parser.ParseCommandLine())
        return NULL;
    if( parser.IsServer() ) {
        ServerConfig server_config;
        if(!parser.ParseServerConfig(&server_config))
            return std::unique_ptr<UdtSocketProfile>();
        else {
            return std::unique_ptr<UdtSocketProfile>(new UdtSocketProfileServer( server_config));
        }
    } else {
        ClientConfig client_config;
        if(!parser.ParseClientConfig(&client_config)) 
            return std::unique_ptr<UdtSocketProfile>();
        else {
            return std::unique_ptr<UdtSocketProfile>(new UdtSocketProfileClient(client_config));
        }
    }
}

