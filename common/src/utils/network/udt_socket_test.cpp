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
#include <atomic>

#include <QCoreApplication>
#include <QCommandLineParser>

#include <utils/network/udt_socket.h>

// A very simple proactor wrapper around UDTSocket/UDTPollSet.
// The major thing here is the proactor NEVER handle error, when it meets
// an error, it just close that socket and delete all the related resources.
// This may simplify our test client program.

namespace {
class Proactor;

template<typename T>
class DualQueue {
public:
    DualQueue():
        front_queue_(&q1_),
        back_queue_(&q2_){}
    std::list<T>* LockBackQueue() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::swap(front_queue_,back_queue_);
        return back_queue_;
    }
    void Push( const T& val ) {
        std::list<T> element;
        element.emplace_back(val);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            front_queue_->splice(front_queue_->end(),std::move(element));
        }
    }
    void Push( T&& val ) {
        std::list<T> element;
        element.emplace_back(std::move(val));
        {
            std::lock_guard<std::mutex> lock(mutex_);
            front_queue_->splice(front_queue_->end(),std::move(element));
        }
    }
private:
    std::list<T> q1_,q2_;
    std::list<T>* front_queue_,*back_queue_;
    std::mutex mutex_;
};

class Ticker {
public:
    Ticker() : 
        last_schedule_time_(-1),
        frequency_(-1) {}
    void SetUp( std::function<void()>&& callback , std::uint32_t frequency ) {
        callback_ = std::move(callback);
        last_schedule_time_ = 0;
        frequency_ = frequency;
    }
    void Tick( std::uint32_t time_offset ) {
        Q_ASSERT(last_schedule_time_ != -1 && frequency_ != -1);
        last_schedule_time_ += time_offset;
        if( last_schedule_time_ >= frequency_ ) {
            last_schedule_time_ -= frequency_;
            callback_();
        }
    }
private:
    std::function<void()> callback_;
    std::uint32_t last_schedule_time_;
    std::uint32_t frequency_;
};

// Events 
enum {
    PENDING_EV_NONE = 0 ,
    PENDING_EV_READ = 1<<0 ,
    PENDING_EV_WRITE = 1<<1 ,
    PENDING_EV_ACCEPT = 1<<2 ,
    PENDING_EV_CONNECT = 1<<3,
    PENDING_EV_REMOVE  = 1<<4,
    PENDING_EV_SHUTDOWN_WORKER = 1<<11,
};

struct PendingEvent {
    UdtSocket* socket;
    int pending_event;
    bool has_exception;
    PendingEvent():pending_event(-1),socket(NULL){}
    PendingEvent( int ev , UdtSocket* sock ) :
        pending_event(ev),
        has_exception(false),
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

struct Remove : public PendingEvent {
    std::function<void(UdtStreamServerSocket*)> callback_server;
    std::function<void(UdtStreamSocket*)> callback_client;
    Remove( std::function<void(UdtStreamSocket*)>&& callback , UdtStreamSocket* socket ) :
        callback_client( std::move(callback) ) , PendingEvent(PENDING_EV_REMOVE,socket){}
    Remove( std::function<void(UdtStreamServerSocket*)>&& callback , UdtStreamServerSocket* socket ) :
        callback_server( std::move(callback) ) , PendingEvent(PENDING_EV_REMOVE,socket){}
};

struct ShutdownWorker : public PendingEvent {
    ShutdownWorker() : PendingEvent( PENDING_EV_SHUTDOWN_WORKER , NULL ) {}
};

class AsyncUdtSocketContextQueue;
class AsyncUdtSocketContext {
public:
    enum {
        PENDING,
        BEFORE_EXECUTION,
        EXECUTION
    };

    int read_lock() const {
        return read_lock_.load(std::memory_order::memory_order_acquire);
    }
    int write_lock() const {
        return write_lock_.load(std::memory_order::memory_order_acquire);
    }
    void set_read_lock( int lk ) {
        return read_lock_.store(lk,std::memory_order::memory_order_release);
    }
    void set_write_lock( int lk ) {
        return write_lock_.store(lk,std::memory_order::memory_order_release);
    }

    class ReadExecutionLock {
    public:
        ReadExecutionLock( AsyncUdtSocketContext* context ):
            context_(context){
            context->set_read_lock( AsyncUdtSocketContext::EXECUTION );
        }
        ~ReadExecutionLock() {
            context_->set_read_lock( AsyncUdtSocketContext::PENDING );
        }
    private:
        AsyncUdtSocketContext* context_;
    };
    class WriteExecutionLock {
    public:
        WriteExecutionLock( AsyncUdtSocketContext* context ):
            context_(context){
                context->set_write_lock( AsyncUdtSocketContext::EXECUTION );
        }
        ~WriteExecutionLock() {
            context_->set_write_lock( AsyncUdtSocketContext::PENDING );
        }
    private:
        AsyncUdtSocketContext* context_;
    };

    UdtSocket* udt_socket();
    PendingEvent* next_read_event() const {
        return next_read_event_;
    }
    PendingEvent* next_write_event() const {
        return next_write_event_;
    }
    bool has_next_read_event() const {
        return next_read_event_ != NULL;
    }
    bool has_next_write_event() const {
        return next_write_event_ != NULL;
    }

    PendingEvent* RemoveReadEvent() {
        PendingEvent* ev = next_read_event_;
        next_read_event_ = NULL;
        return ev;
    }
    PendingEvent* RemoveWriteEvent() {
        PendingEvent* ev = next_write_event_;
        next_write_event_ = NULL;
        return ev;
    }

    // Using these 2 functions to add the pending IO event, and if the 
    // function returns false, it means there's a pending event that
    // haven't been finished, so the user is not allow to post new event
    bool AddReadEvent( PendingEvent* event ) {
        if( read_lock() != EXECUTION )
            return false;
        Q_ASSERT( next_read_event_ == NULL );
        next_read_event_ = event;
        return true;
    }
    bool AddWriteEvent( PendingEvent* event ) {
        if( write_lock() != EXECUTION )
            return false;
        Q_ASSERT( next_write_event_ == NULL );
        next_write_event_ = event;
        return true;
    }
private:
    AsyncUdtSocketContext(  UdtSocket* socket ,
        PendingEvent* read_event,
        PendingEvent* write_event ):
    read_lock_(PENDING),
        write_lock_(PENDING),
        udt_socket_(socket),
        next_read_event_(read_event),
        next_write_event_(write_event) {}

    // Since UDT allows full duplex , therefore 2 lock is needed here 
    std::atomic<int> read_lock_;
    std::atomic<int> write_lock_;

    // UdtSocket pointer
    UdtSocket* udt_socket_;

    // IO event can happen at the same time since the UDT is full duplex
    // Additionally :
    // connect->write
    // accept->read
    PendingEvent* next_read_event_;
    PendingEvent* next_write_event_;

    friend class AsyncUdtSocketContextQueue;
    Q_DISABLE_COPY(AsyncUdtSocketContext)
};

// Dual queue seems not work with the UDT::epoll_set, it is not only a edge triggered, but also
// a none queueable trigger. Therefore, we need to continue the fd registered state until the
// user explicitly tells me to stop it. Therefore a DualQueue which may introduce arbitrary delay
// will not work here. What we gonna do is having a safe shared data structure to store all the
// socket related event information and just manipulate this data structure and reflects the operation
// underwood later on. This is definitely slower but I guess it should be OK , otherwise multiple 
// epoll set must be set up which seems to be an invariant to our existed code base .
class AsyncUdtSocketContextQueue {
public:
    AsyncUdtSocketContextQueue(){}
    // The return pointer is only valid that if the deleted tag is not there
    // otherwise it is not safe to do so and the most important thing is that
    // after you set the delete to true, do not use this pointer anymore .
    std::weak_ptr<AsyncUdtSocketContext> LookUp( UdtSocket* socket ) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::map< UdtSocket* , std::shared_ptr<AsyncUdtSocketContext> >::const_iterator 
            ib = async_socket_map_.find(socket);
        return ib == async_socket_map_.cend() ? std::weak_ptr<AsyncUdtSocketContext>() :
            std::weak_ptr<AsyncUdtSocketContext>(ib->second);
    }
    std::shared_ptr<AsyncUdtSocketContext> CreateRead( UdtSocket* socket , PendingEvent* read_event ) {
        Q_ASSERT(LookUp(socket).expired());
        std::shared_ptr<AsyncUdtSocketContext> context( new AsyncUdtSocketContext(socket,read_event,NULL) );
        {
            std::lock_guard<std::mutex> lock(mutex_);
            async_socket_map_.emplace(socket,context);
        }
        return context;
    }
    std::shared_ptr<AsyncUdtSocketContext> CreateWrite( UdtSocket* socket , PendingEvent* write_event ) {
        Q_ASSERT(LookUp(socket).expired());
        std::shared_ptr<AsyncUdtSocketContext> context( new AsyncUdtSocketContext(socket,NULL,write_event) );
        {
            std::lock_guard<std::mutex> lock(mutex_);
            async_socket_map_.emplace(socket,context);
        }
        return context;
    }
    bool Delete( UdtSocket* socket ) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::map<UdtSocket*,std::shared_ptr<AsyncUdtSocketContext> >::iterator
            ib = async_socket_map_.find(socket);
        if( ib != async_socket_map_.end() ) {
            async_socket_map_.erase(ib);
            return true;
        }
        return false;
    }
private:
    mutable std::mutex mutex_;
    std::map< UdtSocket* , std::shared_ptr<AsyncUdtSocketContext> > async_socket_map_;
    Q_DISABLE_COPY(AsyncUdtSocketContextQueue)
};

// Completion
class CompletionHandleWorker {
public:
    CompletionHandleWorker( Proactor* proactor ) :
        proactor_(proactor) {
            thread_ = std::thread(&CompletionHandleWorker::ThreadMain,this);
    }
    void PostPendingEvent( PendingEvent* ptr ) {
        std::list< PendingEvent* > element(1,ptr);
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
    void DoRead( Read* read );
    void HandleWrite( Write* write );
    void DoWrite( Write* write );
    void HandleConnect( Connect* connect );
    void HandleAccept( Accept* accept );
    void DoAccept( Accept* accept );
    void HandleRemove( Remove* remove );
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
        case PENDING_EV_CONNECT:
            HandleConnect( static_cast<Connect*>(pending_event) );
            break;
        case PENDING_EV_READ:
            HandleRead( static_cast<Read*>(pending_event) );
            break;
        case PENDING_EV_WRITE:
            HandleWrite( static_cast<Write*>(pending_event) );
            break;
        case PENDING_EV_REMOVE:
            HandleRemove( static_cast<Remove*>(pending_event) );
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
public:
    bool NotifyRead( UdtStreamSocket* socket , std::function<void(UdtStreamSocket*,const std::vector<char>&,bool)>&& callback ) ;
    bool NotifyAccept( UdtStreamServerSocket* socket , std::function<void(UdtStreamServerSocket*,const std::vector<UdtStreamSocket*>&,bool)>&& callback);
    bool NotifyWrite( UdtStreamSocket* socket , const std::vector<char>& buffer , std::function<void(UdtStreamSocket*,bool)>&& callback);
    bool NotifyWrite( UdtStreamSocket* socket , std::vector<char>&& buffer , std::function<void(UdtStreamSocket*,bool)>&& callback);
    bool NotifyConnect( UdtStreamSocket* socket , std::function<void(UdtStreamSocket*,bool)>&& callback );
    bool NotifyRemove( UdtStreamSocket* socket , std::function<void(UdtStreamSocket*)>&& callback );
    bool NotifyRemove( UdtStreamServerSocket* socket, std::function<void(UdtStreamServerSocket*)>&& callback );
    bool Run( std::size_t thread_pool_size , int timeout , std::function<void()>&& on_idel = std::function<void()>() );
    void Quit();
private:
    void ExecutePendingIO();
    void InitializeWorkerQueue( std::size_t );

    struct UdtPollSetOperation {
        enum {
            ADD_READ,
            ADD_WRITE,
            ADD,
            REMOVE_READ,
            REMOVE_WRITE,
            REMOVE
        };
        int type;
        UdtSocket* socket;
        PendingEvent* close_event;
        UdtPollSetOperation( UdtSocket* s ) :
            type(ADD),socket(s){}
        UdtPollSetOperation( UdtSocket* s , PendingEvent* ev ) :
            type(REMOVE),socket(s),close_event(ev){}
        UdtPollSetOperation( UdtSocket* s , PendingEvent* ev , int et ) :
            type(et),socket(s),close_event(ev){}
        UdtPollSetOperation( UdtSocket* s , int et ) :
            type(et),socket(s),close_event(NULL){}
    };

    void EPollAddRead( UdtSocket* socket ) {
        udt_epoll_set_operations_.Push( UdtPollSetOperation(socket,UdtPollSetOperation::ADD_READ) );
    }
    void EPollAddWrite( UdtSocket* socket ) {
        udt_epoll_set_operations_.Push( UdtPollSetOperation(socket,UdtPollSetOperation::ADD_WRITE) );
    }
    void EPollAdd( UdtSocket* socket ) {
        udt_epoll_set_operations_.Push( UdtPollSetOperation(socket) );
    }
    void EPollRemoveRead( UdtSocket* socket ) {
        udt_epoll_set_operations_.Push( UdtPollSetOperation(socket,NULL,UdtPollSetOperation::REMOVE_READ) );
    }
    void EPollRemoveWrite( UdtSocket* socket ) {
        udt_epoll_set_operations_.Push( UdtPollSetOperation(socket,NULL,UdtPollSetOperation::REMOVE_WRITE) );
    }
    void EPollRemove( UdtSocket* socket , PendingEvent* close_event ) {
        udt_epoll_set_operations_.Push( UdtPollSetOperation(socket,close_event) );
    }
    void ExecuteEPollOperations();

    // A simple RR scheduler 
    CompletionHandleWorker* ScheduleNextIOCompletionHandle() {
        CompletionHandleWorker* ret = compleition_worker_queue_[finger_].get();
        ++finger_;
        if( finger_ == compleition_worker_queue_.size() ) {
            finger_ = 0;
        }
        return ret;
    }
private:
    AsyncUdtSocketContextQueue udt_socket_queue_;
    UdtPollSet reactor_;
    std::vector< std::unique_ptr<CompletionHandleWorker> > compleition_worker_queue_;
    DualQueue<UdtPollSetOperation> udt_epoll_set_operations_;
    bool quit_;
    int finger_;
    friend class CompletionHandleWorker;
};

void Proactor::ExecuteEPollOperations() {
    std::list<UdtPollSetOperation>* q = udt_epoll_set_operations_.LockBackQueue();
    std::list<UdtPollSetOperation>::iterator ib = q->begin();
    for( ; ib != q->end() ; ++ib ) {
        switch( ib->type ) {
        case UdtPollSetOperation::ADD:
            reactor_.add(ib->socket,aio::etRead,NULL);
            reactor_.add(ib->socket,aio::etWrite,NULL);
            break;
        case UdtPollSetOperation::ADD_READ:
            reactor_.add(ib->socket,aio::etRead,NULL);
            break;
        case UdtPollSetOperation::ADD_WRITE:
            reactor_.add(ib->socket,aio::etWrite,NULL);
            break;
        case UdtPollSetOperation::REMOVE_READ:
            reactor_.remove(ib->socket,aio::etRead);
            break;
        case UdtPollSetOperation::REMOVE_WRITE:
            reactor_.remove(ib->socket,aio::etWrite);
            break;
        case UdtPollSetOperation::REMOVE:
            reactor_.remove(ib->socket,aio::etRead);
            reactor_.remove(ib->socket,aio::etWrite);
            ScheduleNextIOCompletionHandle()->PostPendingEvent(
                ib->close_event);
            break;
        default: Q_ASSERT(0); break;
        }
    }
    // Clear the queue
    q->clear();
}

bool Proactor::NotifyRead( UdtStreamSocket* socket , std::function<void(UdtStreamSocket*,const std::vector<char>&,bool)>&& callback ) {
    std::shared_ptr<AsyncUdtSocketContext> ptr = udt_socket_queue_.LookUp(socket).lock();
    if( !ptr ) {
        ptr = udt_socket_queue_.CreateRead(socket, new Read(std::move(callback),socket));
        // Add the read here
        EPollAddRead(socket);
    } else {
        if(!ptr->AddReadEvent( new Read(std::move(callback),socket) ))
            return false;
    }
    return true;
}

bool Proactor::NotifyAccept( UdtStreamServerSocket* socket , std::function<void(UdtStreamServerSocket*,const std::vector<UdtStreamSocket*>&,bool)>&& callback ) {
    std::shared_ptr<AsyncUdtSocketContext> ptr = udt_socket_queue_.LookUp(socket).lock();
    if( !ptr ) {
        ptr = udt_socket_queue_.CreateRead(socket, new Accept(std::move(callback),socket));
        EPollAddRead(socket);
    } else {
        if(!ptr->AddReadEvent( new Accept(std::move(callback),socket) ))
            return false;
    }
    return true;
}

bool Proactor::NotifyWrite( UdtStreamSocket* socket , const std::vector<char>& buffer , std::function<void(UdtStreamSocket*,bool)>&& callback ) {
    std::shared_ptr<AsyncUdtSocketContext> ptr = udt_socket_queue_.LookUp(socket).lock();
    if( !ptr ) {
        ptr = udt_socket_queue_.CreateWrite(socket,new Write(buffer,std::move(callback),socket));
        EPollAdd(socket);
    } else {
        if(!ptr->AddWriteEvent(new Write(buffer,std::move(callback),socket)))
            return false;
    }
    return true;
}

bool Proactor::NotifyWrite( UdtStreamSocket* socket , std::vector<char>&& buffer , std::function<void(UdtStreamSocket*,bool)>&& callback) {
    std::shared_ptr<AsyncUdtSocketContext> ptr = udt_socket_queue_.LookUp(socket).lock();
    if( !ptr ) {
        ptr = udt_socket_queue_.CreateWrite(socket,new Write(std::move(buffer),std::move(callback),socket));
        EPollAdd(socket);
    } else {
        if(!ptr->AddWriteEvent(new Write(std::move(buffer),std::move(callback),socket)))
            return false;
    }
    return true;
}

bool Proactor::NotifyConnect( UdtStreamSocket* socket , std::function<void(UdtStreamSocket*,bool)>&& callback ) {
    std::shared_ptr<AsyncUdtSocketContext> ptr = udt_socket_queue_.LookUp(socket).lock();
    if( !ptr ) {
        ptr = udt_socket_queue_.CreateWrite(socket,new Connect(std::move(callback),socket) );
        EPollAddWrite(socket);
    } else {
        if(!ptr->AddWriteEvent(new Connect(std::move(callback),socket)))
            return false;
    }
    return true;
}

bool Proactor::NotifyRemove( UdtStreamServerSocket* socket, std::function<void(UdtStreamServerSocket*)>&& callback ) {
    if(!udt_socket_queue_.Delete(socket))
        return false;
    EPollRemove(socket,new Remove(std::move(callback),socket));
    return true;
}

bool Proactor::NotifyRemove( UdtStreamSocket* socket, std::function<void(UdtStreamSocket*)>&& callback ) {
    if(!udt_socket_queue_.Delete(socket))
        return false; 
    EPollRemove(socket,new Remove(std::move(callback),socket));
    return true;
}

void Proactor::ExecutePendingIO() {
    UdtPollSet::const_iterator ib = reactor_.begin();
    for( ; ib != reactor_.end() ; ++ib ) {
        std::shared_ptr<AsyncUdtSocketContext> socket_context =
            udt_socket_queue_.LookUp(ib.socket()).lock();
        if( !socket_context ) {
            continue;
        }
        // Dispatching the event 
        switch( ib.eventType() ) {
        case aio::etRead: 
            if( socket_context->read_lock() == AsyncUdtSocketContext::PENDING && 
                socket_context->has_next_read_event() ) {
                    socket_context->next_read_event()->has_exception = false;
                    socket_context->set_read_lock( AsyncUdtSocketContext::BEFORE_EXECUTION );
                    ScheduleNextIOCompletionHandle()
                        ->PostPendingEvent(socket_context->RemoveReadEvent());
            } 
            break;
        case aio::etWrite:
            if( socket_context->write_lock() == AsyncUdtSocketContext::PENDING && 
                socket_context->has_next_write_event() ) {
                    socket_context->next_write_event()->has_exception = false;
                    socket_context->set_write_lock( AsyncUdtSocketContext::BEFORE_EXECUTION );
                    ScheduleNextIOCompletionHandle()
                        ->PostPendingEvent(socket_context->RemoveWriteEvent());
            } 
            break;
        case aio::etError:
            // For error, we invoke read/write handler to notify the user because we don't
            // know this aio::etError is cuased by the read or by the write here .This may
            // need more consideration, since our deletion operation right now can handle 
            // dummy deletion internally. We don't care how many read/write has been invoked.
            if( socket_context->write_lock() == AsyncUdtSocketContext::PENDING && 
                socket_context->has_next_write_event() ) {
                    socket_context->next_write_event()->has_exception = true;
                    socket_context->set_write_lock( AsyncUdtSocketContext::BEFORE_EXECUTION );
                    ScheduleNextIOCompletionHandle()
                        ->PostPendingEvent(socket_context->RemoveWriteEvent());
            } 
            if( socket_context->read_lock() == AsyncUdtSocketContext::PENDING &&
                socket_context->has_next_read_event() ) {
                    socket_context->next_write_event()->has_exception = true;
                    socket_context->set_read_lock( AsyncUdtSocketContext::BEFORE_EXECUTION );
                    ScheduleNextIOCompletionHandle()
                        ->PostPendingEvent(socket_context->RemoveReadEvent());
            }
            break;
        default: Q_ASSERT(0); break;
        }
    }
}

bool Proactor::Run( std::size_t thread_size , int timeout , std::function<void()>&& on_idel ) {
    InitializeWorkerQueue(thread_size);
    do {
        ExecuteEPollOperations();
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

void CompletionHandleWorker::DoRead( Read* read ) {
    static const int kMaxStackBufferSize = 1024;
    UdtStreamSocket* socket = static_cast<UdtStreamSocket*>(read->socket);
    std::vector<char> buffer;
    read->buffer.clear();
    while (true) {
        buffer.resize(kMaxStackBufferSize);
        int sz = socket->recv(&(*buffer.begin()),kMaxStackBufferSize);
        if( sz < 0 ) {
            read->callback(static_cast<UdtStreamSocket*>(read->socket) , (read->buffer) , false );
            return;
        } else {
            if( sz < kMaxStackBufferSize ) {
                if( sz != 0 ) {
                    buffer.resize(sz);
                    read->buffer.insert( read->buffer.end() , buffer.begin() , buffer.end() );
                }
                read->callback(static_cast<UdtStreamSocket*>(read->socket),(read->buffer),true);
                return;
            }
        }
    }
}

void CompletionHandleWorker::HandleRead( Read* ptr ) {
    std::unique_ptr<Read> read(ptr);
    std::shared_ptr<AsyncUdtSocketContext> socket_context = 
        proactor_->udt_socket_queue_.LookUp(read->socket).lock();
    if( !socket_context )
        return;
    // Lock the read
    AsyncUdtSocketContext::ReadExecutionLock read_lock(socket_context.get());
    PendingEvent* next_write_event = socket_context->next_write_event();
    DoRead(ptr);
    // We never remove the read operation since it will make our framework
    // lost event , so just keep it inside of the epoll set will be a good idea.
    if( socket_context->next_write_event() != next_write_event ){
        // User also wants the write notification
        proactor_->EPollAddWrite(read->socket);
    }
}

void CompletionHandleWorker::DoWrite( Write* write ) {
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
            proactor_->NotifyWrite( static_cast<UdtStreamSocket*>(write->socket) , std::move(write->buffer) , std::move(write->callback) );
        }
    } else {
        write->callback( static_cast<UdtStreamSocket*>(write->socket),false );
    }
}

void CompletionHandleWorker::HandleWrite( Write* ptr ) {
    std::unique_ptr<Write> write(ptr);
    std::shared_ptr<AsyncUdtSocketContext> socket_context = 
        proactor_->udt_socket_queue_.LookUp(write->socket).lock();
    if( !socket_context )
        return;
    AsyncUdtSocketContext::WriteExecutionLock write_lock(socket_context.get());
    Q_ASSERT(socket_context->next_write_event() == NULL);
    DoWrite(ptr);
    if( socket_context->next_write_event() == NULL ) {
        // Remove the write event
        proactor_->EPollRemoveWrite(write->socket);
    }
}

void CompletionHandleWorker::HandleAccept( Accept* ptr ) {
    std::unique_ptr<Accept> accept(ptr);
    std::shared_ptr<AsyncUdtSocketContext> socket_context = 
        proactor_->udt_socket_queue_.LookUp(accept->socket).lock();
    if( !socket_context )
        return;
    AsyncUdtSocketContext::ReadExecutionLock read_lock(socket_context.get());
    PendingEvent* next_write_event = socket_context->next_write_event();
    DoAccept(ptr);
    if( socket_context->next_write_event() != next_write_event ){
        proactor_->EPollAddWrite(accept->socket);
    }
}

void CompletionHandleWorker::DoAccept( Accept* accept ) {
    // checking the pending read buffer size
    UdtStreamServerSocket* server = static_cast<UdtStreamServerSocket*>(accept->socket);
    // accept as much as possible here
    std::vector<UdtStreamSocket*> accept_sockets;
    do {
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
    } while( true );
}

void CompletionHandleWorker::HandleConnect( Connect* ptr ) {
    std::unique_ptr<Connect> connect(ptr);
    std::shared_ptr<AsyncUdtSocketContext> socket_context = 
        proactor_->udt_socket_queue_.LookUp(connect->socket).lock();
    if( !socket_context )
        return;
    // Lock the write
    AsyncUdtSocketContext::WriteExecutionLock write_lock(socket_context.get());

    UdtStreamSocket* socket = static_cast<UdtStreamSocket*>(connect->socket);
    // We cannot safely say that the socket is connected or not 
    connect->callback(socket,!connect->has_exception);
    if( socket_context->next_write_event() == NULL ) {
        // Remove the write event
        proactor_->EPollRemoveWrite(connect->socket);
    }
}

void CompletionHandleWorker::HandleRemove( Remove* ptr ) {
    std::unique_ptr<Remove> remove(ptr);
    if( remove->callback_client ) {
        remove->callback_client( 
            static_cast<UdtStreamSocket*>(remove->socket));
    } else {
        remove->callback_server(
            static_cast<UdtStreamServerSocket*>(remove->socket));
    }
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
        current_conn_size_(0) {
    }
    virtual bool run() {
        // Set up the print statistic function callback 
        ticker_.SetUp( std::bind(&UdtSocketProfileServer::PrintStatistic,this),kPrintStatisticFrequency );
        // Set up the listen fd
        server_socket_.setNonBlockingMode(true);
        if( !server_socket_.bind(SocketAddress( HostAddress(address_) , port_ ) ) ) 
            return false;
        if( !server_socket_.listen() )
            return false;

        std::cout<<"The server("
            <<address_.toStdString()<<":"<<port_<<")"
            <<"starts listening!"<<std::endl;
        // Start accept
        proactor_.NotifyAccept(&server_socket_,
            std::bind(
            &UdtSocketProfileServer::OnAccept,
            this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3));
        PrintStatistic();
        // Block into the poll
        return proactor_.Run( thread_size_ , kMaximumSleepTime ,
            std::bind(
            &Ticker::Tick,
            &ticker_,
            kMaximumSleepTime));
    }
    virtual void quit() {
        proactor_.Quit();
    }
private:
    void HandleError( UdtStreamSocket* socket ) {
        // We simply close this socket here
        proactor_.NotifyRemove(socket,
            std::bind(
            &UdtSocketProfileServer::OnClose,
            this,
            std::placeholders::_1));
    }

    void OnAccept( UdtStreamServerSocket* server , const std::vector<UdtStreamSocket*>& conn , bool ok ) {
        // Register read callback
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
        Q_UNUSED(read);
        if( !ok || read.empty() ) {
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
        conn->close();
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
    static const int kMaximumSleepTime = 400;
    static const std::uint32_t kPrintStatisticFrequency = 1000;
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
    // Ticker
    Ticker ticker_;
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
        active_conn_sockets_size_(0),
        closed_conn_socket_size_(0),
        failed_connection_size_(0){
            prev_position_ = sleep_list_.begin();
    }

    virtual bool run() {
        ticker_.SetUp(std::bind(&UdtSocketProfileClient::PrintStatistic,this),kPrintStatisticFrequency);
        ScheduleConnection();
        std::cout<<"The client starts running!"<<std::endl;
        PrintStatistic();
        return proactor_.Run( thread_size_ , kMaximumSleepTime , 
            std::bind(
            &UdtSocketProfileClient::OnIdle,
            this));
    }

private:
    void HandleError( UdtStreamSocket* socket ) {
        proactor_.NotifyRemove(socket,
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
        socket->close();
        delete socket;
        --active_conn_sockets_size_;
        ++closed_conn_socket_size_;
    }

private:
    // Socket simulation schedule function
    void OnIdle() {
        DoSchedule();
        ticker_.Tick(kMaximumSleepTime);
    }
    void PrintStatistic() {
        std::cout<<"====================================================\n";
        std::cout<<"Active connection:"<<active_conn_sockets_size_<<"\n";
        std::cout<<"Closed connection:"<<closed_conn_socket_size_<<"\n";
        std::cout<<"Failed connection:"<<failed_connection_size_<<"\n";
        std::cout<<"===================================================="<<std::endl;
    }
    void DoSchedule() {
        //ScheduleSleepSockets();
        //ScheduleConnection();
    }
    void ScheduleSleepSockets();
    bool ScheduleSleepSocket( UdtStreamSocket* socket ) {
        if( random_.Roll(active_load_factor_) ) {
            // Schedule this sleepy socket
            ScheduleWrite( socket );
            return true;
        } else if( random_.Roll(disconnect_rate_) ) {
            proactor_.NotifyRemove(socket,
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
    static const int kMaximumSleepTime = 200;
    static const std::uint32_t kPrintStatisticFrequency = 1000;
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
    int failed_connection_size_;
    int closed_conn_socket_size_;
    // the list that cache all the inactive sockets
    std::list<UdtStreamSocket*> sleep_list_;
    // list will not invalid the existed iterator 
    std::list<UdtStreamSocket*>::iterator prev_position_;
    // ticker
    Ticker ticker_;
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
            if(!socket->connect( address_, port_ )) {
                ++failed_connection_size_;
                continue;
            }
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

