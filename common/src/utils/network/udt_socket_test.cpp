#include "udt_socket_test.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <list>
#include <vector>
#include <array>
#include <functional>
#include <cstdint>
#include <random>
#include <ctime>
#include <iostream>

#include <QCoreApplication>
#include <QCommandLineParser>

#include <utils/network/udt_socket.h>

// A very simple proactor wrapper around UDTSocket/UDTPollSet.
// The major thing here is the proactor NEVER handle error, when it meets
// an error, it just close that socket and delete all the related resources.
// This may simplify our test client program.

namespace {

class Proactor;

template< typename T >
class DualQueue {
public:
    DualQueue():
        front_queue_(&q1_),
        back_queue_(&q2_){}
    std::list<T>* LockBackQueue() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::swap(front_queue_,back_queue_);
        }
        return back_queue_;
    }
    void Push( const T& val ) {
        // Using this temporary list trick is good for 
        // 1) move exception possible code outside the lock
        // 2) move slow memory allocation code outside of the lock
        std::list<T> element(1,val);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            front_queue_->splice(front_queue_->end(),std::move(element));
        }
    }
    void Push( T&& val ) {
        std::list<T> element(1,std::move(val));
        {
            std::lock_guard<std::mutex> lock(mutex_);
            front_queue_->splice(front_queue_->end(),std::move(element));
        }
    }
private:
    std::list<T> q1_,q2_;
    std::list<T>* front_queue_ , *back_queue_;
    std::mutex mutex_;
};

// Events 

enum {
    PENDING_EV_READ,
    PENDING_EV_WRITE,
    PENDING_EV_ACCEPT,
    PENDING_EV_CONNECT,
    PENDING_EV_CLOSE,
    PENDING_EV_SHUTDOWN_WORKER
};

struct PendingEvent {
    int pending_event;
    UdtSocket* socket;
    PendingEvent():pending_event(-1),socket(NULL){}
    PendingEvent( int ev , UdtSocket* sock ) :
        pending_event(ev),
        socket(sock){}
};

struct Read : public PendingEvent {
    std::vector<char> buffer;
    std::function<void(UdtStreamSocket*,const std::vector<char>&,bool)> callback;
    Read( std::function<void(UdtStreamSocket*,const std::vector<char>&,bool)>&& cb , UdtStreamSocket* socket ) :
         callback( std::move(cb) ),PendingEvent(PENDING_EV_READ,socket) {}

};

struct Write : public PendingEvent {
    std::vector<char> buffer;
    std::function<void(UdtStreamSocket*,bool)> callback;
    Write( std::vector<char>&& b , std::function<void(UdtStreamSocket*,bool)>&& cb , UdtStreamSocket* socket ) :
        buffer(std::move(b)),callback( std::move(cb) ),PendingEvent(PENDING_EV_WRITE,socket) {}
    Write( const std::vector<char>& b , std::function<void(UdtStreamSocket*,bool)>&& cb , UdtStreamSocket* socket ) :
        buffer(b),callback( std::move(cb) ),PendingEvent(PENDING_EV_WRITE,socket) {}
};

struct Connect : public PendingEvent {
    std::function<void(UdtStreamSocket*,bool)> callback;
    Connect( std::function<void(UdtStreamSocket*,bool)>&& cb , UdtStreamSocket* socket ) :
        callback(std::move(cb)) , PendingEvent(PENDING_EV_CONNECT,socket){}
};

struct Accept : public PendingEvent {
    std::function<void(UdtStreamServerSocket*,const std::vector<UdtStreamSocket*>&,bool)> callback;
    Accept( std::function<void(UdtStreamServerSocket*,const std::vector<UdtStreamSocket*>&,bool)>&& cb , UdtSocket* socket ) :
        callback(std::move(cb)) , PendingEvent(PENDING_EV_ACCEPT,socket){}
};

struct Close : public PendingEvent {
    std::function<void(UdtStreamSocket*)> callback;
    Close( std::function<void(UdtStreamSocket*)>&& cb , UdtStreamSocket* socket ) :
        callback(std::move(cb)) , PendingEvent( PENDING_EV_CLOSE , socket ) {}
};

struct ShutdownWorker : public PendingEvent {
    ShutdownWorker() : PendingEvent( PENDING_EV_SHUTDOWN_WORKER , NULL ) {}
};

// Completion

class CompletionHandleWorker {
public:
    CompletionHandleWorker( Proactor* proactor ) :
        proactor_(proactor) {
            thread_ = std::thread(&CompletionHandleWorker::ThreadMain,this);
    }
    void PostPendingEvent( PendingEvent* ptr ) {
        std::unique_ptr<PendingEvent> event(ptr);
        std::list< PendingEvent* > element(1,event.release());
        {
            std::lock_guard<std::mutex> lock(mutex_);
            event_queue_.splice(event_queue_.end(),std::move(element));
        }
        cv_.notify_all();
    }
    void Quit() {
        PostPendingEvent(new ShutdownWorker);
        thread_.join();
    }
private:
    void HandleRead( Read* read );
    void HandleWrite( Write* write );
    void HandleConnect( Connect* connect );
    void HandleAccept( Accept* accept );
    void HandleClose( Close* close );
    void DeleteSocket( UdtStreamSocket* socket ) {
        delete socket;
    }
    void ThreadMain();
private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread thread_;
    std::list< PendingEvent* > event_queue_;
    Proactor* proactor_;
};

void CompletionHandleWorker::ThreadMain() {
    do {
        PendingEvent* pending_event;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            while( event_queue_.empty() ) {
                cv_.wait(lock);
            }
            pending_event = event_queue_.front();
            event_queue_.pop_front();
        }
        switch( pending_event->pending_event ) {
        case PENDING_EV_ACCEPT:
            HandleAccept( static_cast<Accept*>(pending_event) );
            break;
        case PENDING_EV_CLOSE:
            HandleClose( static_cast<Close*>(pending_event) );
            break;
        case PENDING_EV_CONNECT:
            HandleConnect( static_cast<Connect*>(pending_event) );
            break;
        case PENDING_EV_READ:
            HandleRead( static_cast<Read*>(pending_event) );
            break;
        case PENDING_EV_WRITE:
            HandleWrite( static_cast<Write*>(pending_event) );
            break;
        case PENDING_EV_SHUTDOWN_WORKER:
            delete pending_event;
            return;
        default: Q_ASSERT(0); return;
        }
    } while(true);
}

// Proactor

class Proactor {
public:
    Proactor() :
        quit_(false),
        finger_(0){}
    void NotifyRead( UdtStreamSocket* socket , std::function<void(UdtStreamSocket*,const std::vector<char>&,bool)>&& callback ) {
        queue_.Push(  new Read(std::move(callback),socket) );
    }
    void NotifyAccept( UdtStreamServerSocket* socket , std::function<void(UdtStreamServerSocket*,const std::vector<UdtStreamSocket*>&,bool)>&& callback) {
        queue_.Push(  new Accept(std::move(callback),socket) );
    }
    void NotifyWrite( UdtStreamSocket* socket , const std::vector<char>& buffer , std::function<void(UdtStreamSocket*,bool)>&& callback) {
        queue_.Push(  new Write(buffer,std::move(callback),socket) );
    }
    void NotifyWrite( UdtStreamSocket* socket , std::vector<char>&& buffer , std::function<void(UdtStreamSocket*,bool)>&& callback) { 
        queue_.Push( new Write(std::move(buffer),std::move(callback),socket) );
    }
    void NotifyConnect( UdtStreamSocket* socket , std::function<void(UdtStreamSocket*,bool)>&& callback ) {
        queue_.Push( new Connect(std::move(callback),socket) );
    }
    void NotifyClose( UdtStreamSocket* socket , std::function<void(UdtStreamSocket*)>&& callback ) {
         queue_.Push( new Close(std::move(callback),socket) );
    }
    bool Run( std::size_t thread_pool_size , int timeout , std::function<void()>&& on_idel = std::function<void()>() );
    void Quit();
private:
    void PreparePendingIO();
    void ExecutePendingIO();
    void InitializeWorkerQueue( std::size_t );
    // A simple RR scheduler 
    CompletionHandleWorker* ScheduleNextIOCompletionHandle() {
        CompletionHandleWorker* ret = compleition_worker_queue_[finger_].get();
        ++finger_;
        if( finger_ == compleition_worker_queue_.size() ) {
            finger_ = 0;
        }
        return ret;
    }
    DualQueue< PendingEvent* > queue_;
    UdtPollSet reactor_;
    std::vector< std::unique_ptr<CompletionHandleWorker> > compleition_worker_queue_;
    bool quit_;
    int finger_;
};

void Proactor::PreparePendingIO() {
    std::list<PendingEvent*>* q = queue_.LockBackQueue();
    // ignore the current status of the back_queue_
    std::list<PendingEvent*>::iterator ib = q->begin();
    for( ; ib != q->end() ; ++ib ) {
        // just push all the pending events into the poller sets here
        switch( (*ib)->pending_event ) {
        case PENDING_EV_ACCEPT:
            reactor_.add( (*ib)->socket , aio::etRead , *ib );
            break;
        case PENDING_EV_CLOSE:
            ScheduleNextIOCompletionHandle()->PostPendingEvent(*ib);
            break;
        case PENDING_EV_WRITE:
            reactor_.add( (*ib)->socket , aio::etWrite , *ib );
            break;
        case PENDING_EV_READ:
            reactor_.add( (*ib)->socket , aio::etRead , *ib );
            break;
        case PENDING_EV_CONNECT:
            reactor_.add( (*ib)->socket , aio::etWrite , *ib );
            break;
        default: Q_ASSERT(0); break;
        }
    }
    q->clear();
}

void Proactor::ExecutePendingIO() {
    UdtPollSet::const_iterator ib = reactor_.begin();
    for( ; ib != reactor_.end() ; ++ib ) {
        void* user_data = ib.userData();
        // Remove the descriptor 
        reactor_.remove( ib.socket() , ib.eventType() );
        // Schedule the completion IO operation
        ScheduleNextIOCompletionHandle()->PostPendingEvent(
            reinterpret_cast<PendingEvent*>(user_data));
    }
}

bool Proactor::Run( std::size_t thread_size , int timeout , std::function<void()>&& on_idel ) {
    InitializeWorkerQueue(thread_size);
    do {
        PreparePendingIO();
        int what = reactor_.poll( timeout );
        if( what < 0 ) 
            return false;
        if( what > 0 ) {
            ExecutePendingIO();
        }
        if( on_idel )
            on_idel();
    } while(!quit_);
    return true;
}

void Proactor::Quit() {
    quit_ = true;
    for( std::size_t i = 0  ; i < compleition_worker_queue_.size() ; ++i ) {
        compleition_worker_queue_[i]->Quit();
    }
}

void Proactor::InitializeWorkerQueue( std::size_t thread_size ) {
    for( std::size_t i  = 0 ; i < thread_size ; ++i ) {
        compleition_worker_queue_.emplace_back( std::unique_ptr<CompletionHandleWorker>(new CompletionHandleWorker(this)) );
    }
}

void CompletionHandleWorker::HandleRead( Read* ptr ) {
    // Read all the data from the kernel and then invoke the user's
    // callback function in this thread 
    static const int kMaxStackBufferSize = 1024;
    std::unique_ptr<Read> read(ptr);
    UdtStreamSocket* socket = static_cast<UdtStreamSocket*>(read->socket);
    std::vector<char> buffer;
    read->buffer.clear();
    while (true) {
        buffer.resize(kMaxStackBufferSize);
        int sz = socket->recv(&(*buffer.begin()),kMaxStackBufferSize);
        if( sz == 0 ) {
            if( read->buffer.empty() ) {
                read->callback( static_cast<UdtStreamSocket*>(read->socket) , (read->buffer) , false );
            } else {
                return;
            }
        } else {
            if( sz <0 ) {
                read->callback(static_cast<UdtStreamSocket*>(read->socket) , (read->buffer) , false );
                return;
            } else {
                buffer.resize(sz);
                read->buffer.insert( read->buffer.end() , buffer.begin() , buffer.end() );
                if( sz < kMaxStackBufferSize ) {
                    read->callback( static_cast<UdtStreamSocket*>(read->socket) , (read->buffer) , true );
                    return;
                }
                // continue
            }
        }
    }
}

void CompletionHandleWorker::HandleWrite( Write* ptr ) {
    std::unique_ptr<Write> write(ptr);
    UdtStreamSocket* socket = static_cast<UdtStreamSocket*>(write->socket);
    int bytes = socket->send(&(*write->buffer.begin()),static_cast<int>(write->buffer.size()));
    if( bytes > 0 ) {
        std::vector<char>::iterator ib = write->buffer.begin();
        std::advance(ib,bytes);
        std::vector<char> temp( ib , write->buffer.end() );
        write->buffer.swap(temp);
        if( write->buffer.empty() ) {
            write->callback( static_cast<UdtStreamSocket*>(write->socket),true);
        } else {
            // still working on write
            proactor_->NotifyWrite( static_cast<UdtStreamSocket*>(ptr->socket) , std::move(write->buffer) , std::move(ptr->callback) );
        }
    } else {
        // error 
        write->callback( static_cast<UdtStreamSocket*>(write->socket),false );
    }
}

void CompletionHandleWorker::HandleAccept( Accept* ptr ) {
    std::unique_ptr<Accept> accept(ptr);
    UdtStreamServerSocket* server = static_cast<UdtStreamServerSocket*>(accept->socket);
    // accept as much as possible here
    std::vector<UdtStreamSocket*> accept_sockets;
    do {
        // The UDT has bug, it doesn't remove the accepted sockets, I don't know
        // why I cannot call the server->accept in a loop instead of painfully let
        // poll notify us and get only _ONE_ 
        UdtStreamSocket* socket = static_cast<UdtStreamSocket*>(server->accept());
        if( socket != NULL ) {
            accept_sockets.push_back(socket);
            socket->setNonBlockingMode(true);
        } else {
            if( accept_sockets.empty() ) {
                accept->callback(server,accept_sockets,false);
            } else {
                accept->callback(server,accept_sockets,true);
            }
            return;
        }
    } while(true);
}

void CompletionHandleWorker::HandleConnect( Connect* ptr ) {
    std::unique_ptr<Connect> connect(ptr);
    UdtStreamSocket* socket = static_cast<UdtStreamSocket*>(connect->socket);
    // We cannot safely say that the socket is connected or not 
    connect->callback(socket,true);
}

void CompletionHandleWorker::HandleClose( Close* ptr ) {
    std::unique_ptr<Close> close(ptr);
    UdtStreamSocket* socket = static_cast<UdtStreamSocket*>(close->socket);
    socket->close();
    close->callback(socket);
}

// Configuration or client/server

struct ServerConfig {
    std::size_t thread_size;
    QString address;
    int port;
};

struct ClientConfig {
    std::size_t thread_size;
    QString address;
    int port;
    // statistics for simulation
    float active_loader_rate;
    float disconnect_rate;
    std::uint32_t maximum_connection_size;
    int content_min_size;
    int content_max_size;
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
     thread_size_(config.thread_size),
     address_(config.address),
     port_(config.port),
     current_conn_size_(0) { }
    virtual bool run() {
        server_socket_.setNonBlockingMode(true);
        if( !server_socket_.bind(SocketAddress( HostAddress(address_) , port_ ) ) ) 
            return false;
        if( !server_socket_.listen() )
            return false;

        std::cout<<"The server("
            <<address_.toStdString()<<":"<<port_<<")"
            <<"starts listening!"<<std::endl;

        proactor_.NotifyAccept(&server_socket_,
            std::bind(
            &UdtSocketProfileServer::OnAccept,
            this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3));
        static const int kMaximumSleepTime = 400;
        return proactor_.Run( thread_size_ , kMaximumSleepTime ,
            std::bind(
            &UdtSocketProfileServer::PrintStatistic,
            this));
    }
    virtual void quit() {
        proactor_.Quit();
    }
private:
    void HandleError( UdtStreamSocket* socket ) {
        // We simply close this socket here
        proactor_.NotifyClose(socket,
            std::bind(
            &UdtSocketProfileServer::OnClose,
            this,
            std::placeholders::_1));
    }
    void OnAccept( UdtStreamServerSocket* server , const std::vector<UdtStreamSocket*>& conn , bool ok ) {
        // Register Read callback
        if(ok) {
            for( std::size_t i = 0 ; i < conn.size() ; ++i ) {
                proactor_.NotifyRead(
                    conn[i],
                    std::bind(
                    &UdtSocketProfileServer::OnRead,
                    this,
                    std::placeholders::_1,
                    std::placeholders::_2,
                    std::placeholders::_3));
            }
            current_conn_size_ += static_cast<int>(conn.size());
        }
        // Continue Accept
        proactor_.NotifyAccept(server,
            std::bind(
            &UdtSocketProfileServer::OnAccept,
            this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3));
    }
    void OnRead( UdtStreamSocket* conn , const std::vector<char>& read , bool ok ) {
        // ignore what we have read.
        Q_UNUSED(read);
        if( !ok ) {
            HandleError(conn);
            return;
        }
        proactor_.NotifyRead(
            conn,
            std::bind(
            &UdtSocketProfileServer::OnRead,
            this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3));
    }
    void OnClose( UdtStreamSocket* conn ) {
        delete conn;
        --current_conn_size_;
    }
    // This function will be called in idle time but not 
    // sure whether it gonna be called or not
    void PrintStatistic() {
        std::cout<<"====================================================\n";
        std::cout<<"Active connection:"<<current_conn_size_<<"\n";
        std::cout<<"===================================================="<<std::endl;
    }
private:
    // Server socket
    UdtStreamServerSocket server_socket_;
    // Proactor
    Proactor proactor_;
    // Configuration parameters
    std::size_t thread_size_;
    QString address_;
    int port_;
    // Statistic 
    int current_conn_size_;

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
    std::vector<char> RandomString( int length ) {
        std::vector<char> ret;
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
        thread_size_(config.thread_size),
        address_(config.address),
        port_(config.port),
        active_load_factor_(config.active_loader_rate),
        disconnect_rate_(config.disconnect_rate),
        maximum_allowed_content_(config.content_max_size),
        minimum_allowed_content_(config.content_min_size),
        maximum_allowed_concurrent_connection_(config.maximum_connection_size),
        active_conn_sockets_size_(0){
            prev_position_ = sleep_list_.begin();
    }
    virtual bool run() {
        static const int kMaximumSleepTime = 200;
        ScheduleConnection();
        std::cout<<"The client starts running!"<<std::endl;
        return proactor_.Run( thread_size_ , kMaximumSleepTime , 
            std::bind(
            &UdtSocketProfileClient::OnIdle,
            this));
    }
private:
    void HandleError( UdtStreamSocket* socket ) {
        proactor_.NotifyClose(socket,
            std::bind(
            &UdtSocketProfileClient::OnClose,
            this,
            std::placeholders::_1));
    }
    virtual void quit() {
        proactor_.Quit();
    }
    void ScheduleWrite( UdtStreamSocket* socket ) {
        // We can write data to the user
        int length = random_.RandomRange(minimum_allowed_content_,maximum_allowed_content_);
        // Send the content
        proactor_.NotifyWrite( socket , std::move(random_.RandomString(length)) ,
            std::bind(
            &UdtSocketProfileClient::OnWrite,
            this,
            std::placeholders::_1,
            std::placeholders::_2));
    }
    void OnConnect( UdtStreamSocket* socket , bool ok) {
        if(!ok) {
            HandleError(socket);
            return;
        }
        if( random_.Roll(active_load_factor_) ) {
            ScheduleWrite(socket);
        } else {
            // Add the socket to sleep_list_ for later schedule
            sleep_list_.push_back( socket );
        }
    }
    void OnWrite( UdtStreamSocket* socket , bool ok ) {
        if(!ok) {
            HandleError(socket);
            return;
        }
        // We have finished our write, so added our self to the sleeped_list_ again
        sleep_list_.push_back( socket );
    }
    void OnClose( UdtStreamSocket* socket ) {
        delete socket;
        --active_conn_sockets_size_;
    }

private:
    // Socket simulation schedule function
    void OnIdle() {
        DoSchedule();
        PrintStatistic();
    }
    void PrintStatistic() {
        std::cout<<"====================================================\n";
        std::cout<<"Active connection:"<<active_conn_sockets_size_<<"\n";
        std::cout<<"===================================================="<<std::endl;
    }
    void DoSchedule() {
        ScheduleSleepSockets();
        ScheduleConnection();
    }
    void ScheduleSleepSockets();
    bool ScheduleSleepSocket( UdtStreamSocket* socket ) {
        if( random_.Roll(active_load_factor_) ) {
            // Schedule this sleepy socket
            ScheduleWrite( socket );
            return true;
        } else if( random_.Roll(disconnect_rate_) ) {
            proactor_.NotifyClose(socket,
                std::bind(
                &UdtSocketProfileClient::OnClose,
                this,
                std::placeholders::_1));
            return true;
        }
        return false;
    }
    void ScheduleConnection();
    float GetConnectionProbability() const {
        const int half_max = maximum_allowed_concurrent_connection_/2;
        if( active_conn_sockets_size_ < half_max )
            return 1.0f;
        else {
            int left = maximum_allowed_concurrent_connection_ - active_conn_sockets_size_;
            Q_ASSERT( left>=0 );
            const float lower_bound = 0.7f;
            return lower_bound + (left/half_max)*(1.0f-lower_bound);
        }
    }

private:
    std::size_t thread_size_;
    QString address_;
    int port_;
    RandomHelper random_;
    Proactor proactor_;
    // statistic configuration
    float active_load_factor_;
    float disconnect_rate_;
    int maximum_allowed_content_;
    int minimum_allowed_content_;
    int maximum_allowed_concurrent_connection_;
    int active_conn_sockets_size_;

    // the list that cache all the inactive sockets
    std::list<UdtStreamSocket*> sleep_list_;
    // list will not invalid the existed iterator 
    std::list<UdtStreamSocket*>::iterator prev_position_;
};

void UdtSocketProfileClient::ScheduleSleepSockets() {
    static const std::size_t kMaximumIterationSize = 128;
    std::size_t iterate_size = kMaximumIterationSize;
    for( ; prev_position_ != sleep_list_.end() && iterate_size > 0 ; --iterate_size ) {
        if( ScheduleSleepSocket(*prev_position_) ) {
            prev_position_ = sleep_list_.erase(prev_position_);
        } else {
            ++prev_position_;
        }
    }
    if( iterate_size > 0 ) {
        Q_ASSERT( prev_position_ == sleep_list_.end() );
        prev_position_ = sleep_list_.begin();
        for( ; prev_position_ != sleep_list_.end() && iterate_size > 0 ; --iterate_size ) {
            if( ScheduleSleepSocket(*prev_position_) ) {
                prev_position_ = sleep_list_.erase(prev_position_);
            } else {
                ++prev_position_;
            }
        }
    }
}

void UdtSocketProfileClient::ScheduleConnection() {
    // Schedule the connection
    for( std::size_t i = active_conn_sockets_size_ ; i < maximum_allowed_concurrent_connection_ ; ++i ) {
        if( random_.Roll( GetConnectionProbability() ) ) {
            std::unique_ptr<UdtStreamSocket> socket( new UdtStreamSocket() );
            socket->setNonBlockingMode(true);
            if(!socket->connect( address_, port_ ))
                continue;
            proactor_.NotifyConnect(socket.release(),
                std::bind(
                &UdtSocketProfileClient::OnConnect,
                this,
                std::placeholders::_1,
                std::placeholders::_2));
            ++active_conn_sockets_size_;
        }
    }
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
    parser_.addOption(
        QCommandLineOption(QLatin1String("addr"),QLatin1String("IPV4 address"),QLatin1String("addr")));
    parser_.addOption(
        QCommandLineOption(QLatin1String("port"),QLatin1String("Port number"),QLatin1String("port")));
    parser_.addOption(
        QCommandLineOption(QLatin1String("thread_size"),QLatin1String("Thread pool size"),QLatin1String("thread_size")));
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
    parser_.process(application_);
    return true;
}

bool CommandLineParser::ParseServerConfig( ServerConfig* config ) {
    Q_ASSERT(IsServer());
    if( !parser_.isSet(QLatin1String("addr")) || 
        !parser_.isSet(QLatin1String("port")) ||
        !parser_.isSet(QLatin1String("thread_size")) ) {
            std::cout<<parser_.helpText().toLatin1().constData()<<std::endl;
            return false;
    } else {
        config->address = parser_.value(QLatin1String("addr"));
        config->port = parser_.value(QLatin1String("port")).toInt();
        if( config->port == 0 ) {
            std::cout<<parser_.helpText().toLatin1().constData()<<std::endl;
            return false;
        }
        config->thread_size = parser_.value(QLatin1String("thread_size")).toUInt();
        if( config->thread_size == 0 ) {
            std::cout<<parser_.helpText().toLatin1().constData()<<std::endl;
            return false;
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
        !parser_.isSet(QLatin1String("thread_size")) ||
        !parser_.isSet(QLatin1String("active_rate")) ||
        !parser_.isSet(QLatin1String("disconn_rate")) ||
        !parser_.isSet(QLatin1String("max_conn")) ) {
            std::cout<<parser_.helpText().toLatin1().constData()<<std::endl;
            return false;
    } else {
        config->address = parser_.value(QLatin1String("addr"));
        config->port = parser_.value(QLatin1String("port")).toInt();
        if( config->port == 0 ) {
            std::cout<<parser_.helpText().toLatin1().constData()<<std::endl;
            return false;
        }
        config->thread_size = parser_.value(QLatin1String("thread_size")).toUInt();
        if( config->thread_size == 0 ) {
            std::cout<<parser_.helpText().toLatin1().constData()<<std::endl;
            return false;
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

