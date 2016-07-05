
#include "ssl_socket.h"

#ifdef ENABLE_SSL
#include <mutex>
#include <errno.h>
#include <list>
#include <memory>
#include <atomic>
#include <thread>

#include <openssl/err.h>
#include <openssl/opensslconf.h>
#include <openssl/ssl.h>

#include <nx/utils/log/log.h>
#include <nx/utils/type_utils.h>
#include <nx/utils/std/future.h>
#include <utils/common/systemerror.h>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

//#define DEBUG_SSL

static const std::size_t kSslAsyncRecvBufferSize(1024 * 100);
static const nx::String kSslSessionId("Network Optix SSL socket");

namespace {

class OpenSslGlobalLockManager;
static std::once_flag kOpenSSLGlobalLockFlag;
static std::unique_ptr<OpenSslGlobalLockManager> openSslGlobalLockManagerInstance;

// SSL global lock. This is a must even if the compilation has configured with THREAD for OpenSSL.
// Based on the documentation of OpenSSL, it internally uses lots of global data structure. Apart
// from this, I have suffered the wired access violation when not give OpenSSL lock callback. The
// documentation says 2 types of callback is needed, however the other one, thread id, is not a
// must since OpenSSL configured with thread support will give default version. Additionally, the
// dynamic lock interface is not used in current OpenSSL version. So we don't use it.
class OpenSslGlobalLockManager
{
public:
    typedef void(*OpenSslLockingCallbackType)(
        int mode, int type, const char *file, int line);

    std::unique_ptr<std::mutex[]> m_openSslGlobalLock;

    OpenSslGlobalLockManager()
    :
        m_initialLockingCallback(CRYPTO_get_locking_callback())
    {
        NX_ASSERT(!m_openSslGlobalLock);
        // not safe here, new can throw exception
        m_openSslGlobalLock.reset(new std::mutex[CRYPTO_num_locks()]);
        CRYPTO_set_locking_callback(&OpenSslGlobalLockManager::openSSLGlobalLock);
    }

    ~OpenSslGlobalLockManager()
    {
        CRYPTO_set_locking_callback(m_initialLockingCallback);
        m_initialLockingCallback = nullptr;
    }

    static void openSSLGlobalLock(int mode, int type, const char* file, int line)
    {
        Q_UNUSED(file);
        Q_UNUSED(line);

        auto& lock = openSslGlobalLockManagerInstance->m_openSslGlobalLock;
        NX_ASSERT(lock);

        if (mode & CRYPTO_LOCK)
            lock.get()[type].lock();
        else
            lock.get()[type].unlock();
    }

private:
    OpenSslLockingCallbackType m_initialLockingCallback;
};

void initOpenSSLGlobalLock()
{
    std::call_once(
        kOpenSSLGlobalLockFlag,
        []() { openSslGlobalLockManagerInstance.reset(new OpenSslGlobalLockManager()); });
}

} // namespace

namespace nx {
namespace network {

class SslAsyncOperation
{
public:
    enum
    {
        PENDING,
        EXCEPTION,
        SUCCESS,
        END_OF_STREAM
    };

    virtual void perform(int* sslReturn, int* sslError) = 0;

    void increasePendingIOCount()
    {
        ++m_pendingIoCount;
    }

    void decreasePendingIOCount()
    {
        NX_ASSERT(m_pendingIoCount !=0);
        --m_pendingIoCount;
        if (m_pendingIoCount == 0) {
            // Check if we can invoke the IO operations, if the IO
            // status is not PENDING, then we can invoke such async
            // SSL operation's user callback function here
            if (m_exitStatus != PENDING)
                invokeUserCallback();
        }
    }

    void setExitStatus(int exitStatus, SystemError::ErrorCode errorCode)
    {
        if (m_exitStatus == PENDING) {
            m_exitStatus = exitStatus;
            m_errorCode = errorCode;
            if (m_pendingIoCount == 0) {
                invokeUserCallback();
            }
        }
    }

    SslAsyncOperation(SSL* ssl) :
        m_pendingIoCount(0),
        m_exitStatus(PENDING),
        m_errorCode(SystemError::noError),
        m_ssl(ssl)
    {}

    virtual ~SslAsyncOperation() {}

protected:
    virtual void invokeUserCallback() = 0;

    void reset()
    {
        m_pendingIoCount = 0;
        m_exitStatus = PENDING;
        m_errorCode = SystemError::noError;
    }

    int m_pendingIoCount;
    int m_exitStatus;
    SystemError::ErrorCode m_errorCode;
    SSL* m_ssl;
};

class SslAsyncRead : public SslAsyncOperation
{
public:
    virtual void perform(int* sslReturn, int* sslError)
    {
        int old_size = m_readBuffer->size();
        int read_size = m_readBuffer->capacity() - old_size;
        m_readBuffer->resize(m_readBuffer->capacity());
        *sslReturn = SSL_read(m_ssl,m_readBuffer->data()+old_size,read_size);
        *sslError = SSL_get_error(m_ssl,*sslReturn);
        if (*sslReturn<=0) {
            m_readBuffer->resize(old_size);
        } else {
            m_readBuffer->resize(old_size+*sslReturn);
            m_readBytes += *sslReturn;
        }
        #ifdef DEBUG_SSL
            NX_LOGX(lm("return %1, error %2").arg(*sslReturn).arg(*sslError),
                (*sslReturn == 1) ? cl_logDEBUG2 : cl_logDEBUG2);
        #endif
    }

    void reset(
        nx::Buffer* buffer,
        std::function<void(SystemError::ErrorCode,std::size_t)>&& handler)
    {
        m_readBuffer = buffer;
        m_handler = std::move(handler);
        m_readBytes = 0;
        SslAsyncOperation::reset();
    }

    SslAsyncRead(SSL* ssl) : SslAsyncOperation(ssl) {}

protected:
    virtual void invokeUserCallback()
    {
        const auto handler = std::move(m_handler);
        m_handler = nullptr;

        switch(m_exitStatus) {
        case SslAsyncOperation::EXCEPTION:
            return handler(m_errorCode, -1);
        case SslAsyncOperation::SUCCESS:
            return handler(SystemError::noError, m_readBytes);
        case SslAsyncOperation::END_OF_STREAM:
            NX_ASSERT(m_readBytes == 0);
            return handler(SystemError::noError, 0);
        default:
            NX_ASSERT(false);
        }
    }

private:
    nx::Buffer* m_readBuffer;
    std::function<void(SystemError::ErrorCode,std::size_t)> m_handler;
    std::size_t m_readBytes;
};

class SslAsyncWrite : public SslAsyncOperation
{
public:
    virtual void perform(int* sslReturn, int* sslError)
    {
        NX_ASSERT(!m_writeBuffer->isEmpty());
        *sslReturn = SSL_write(
            m_ssl,m_writeBuffer->constData(),m_writeBuffer->size());
        *sslError = SSL_get_error(m_ssl,*sslReturn);

        #ifdef DEBUG_SSL
            NX_LOGX(lm("return %1, error %2").arg(*sslReturn).arg(*sslError),
                (*sslReturn == 1) ? cl_logDEBUG2 : cl_logDEBUG1);
        #endif
    }

    void reset(
        const nx::Buffer* buffer,
        std::function<void(SystemError::ErrorCode,std::size_t)>&& handler)
    {
        m_writeBuffer = buffer;
        m_handler = std::move(handler);
        SslAsyncOperation::reset();
    }

    SslAsyncWrite(SSL* ssl) : SslAsyncOperation(ssl) {}

protected:
    virtual void invokeUserCallback()
    {
        const auto handler = std::move(m_handler);
        m_handler = nullptr;

        switch(m_exitStatus) {
        case SslAsyncOperation::EXCEPTION:
            return handler(SystemError::connectionAbort, -1);
        case SslAsyncOperation::END_OF_STREAM:
            return handler(SystemError::noError, 0);
        case SslAsyncOperation::SUCCESS:
            return handler(SystemError::noError, m_writeBuffer->size());
        default:
            NX_ASSERT(false);
        }
    }

private:
    const nx::Buffer* m_writeBuffer;
    std::function<void(SystemError::ErrorCode,std::size_t)> m_handler;
};

class SslAsyncHandshake : public SslAsyncOperation
{
public:
    virtual void perform(int* sslReturn, int* sslError)
    {
        *sslReturn = SSL_do_handshake(m_ssl);
        *sslError = SSL_get_error(m_ssl,*sslReturn);

        #ifdef DEBUG_SSL
            NX_LOGX(lm("return %1, error %2").arg(*sslReturn).arg(*sslError),
                (*sslReturn == 1) ? cl_logDEBUG2 : cl_logDEBUG1);
        #endif
    }

    void reset(std::function<void(SystemError::ErrorCode)>&& handler)
    {
        m_handler = std::move(handler);
        SslAsyncOperation::reset();
    }

    SslAsyncHandshake(SSL* ssl) : SslAsyncOperation(ssl) {}

protected:
    virtual void invokeUserCallback()
    {
        const auto handler = std::move(m_handler);
        m_handler = nullptr;

        switch(m_exitStatus) {
        case SslAsyncOperation::EXCEPTION:
            return handler(SystemError::connectionAbort);
        case SslAsyncOperation::END_OF_STREAM:
            return handler(SystemError::connectionReset);
        case SslAsyncOperation::SUCCESS:
            return handler(SystemError::noError);
        default:
            NX_ASSERT(false);
        }
    }

private:
    std::function<void(SystemError::ErrorCode)> m_handler;
};

class SslAsyncBioHelper
{
public:
    SslAsyncBioHelper(
        SSL* ssl, AbstractStreamSocket* underly_socket, bool is_server);

    ~SslAsyncBioHelper();

public:
    // BIO operation function. These 2 BIO operation function will simulate BIO memory
    // type. For read, it will simply checks the input read buffer, and for write operations
    // it will write the data into the write buffer and then the upper will detect what
    // needs to send out. This style makes the code easier and simpler and also since we
    // have PendingIOCount, all the user callback function will be invoked in proper order
    std::size_t bioWrite(const void* data, std::size_t size);
    std::size_t bioRead(void* data, std::size_t size);

    bool eof() const {
        return m_eof;
    }

    bool asyncSend(
        const nx::Buffer& buffer,
        std::function<void(SystemError::ErrorCode,std::size_t)>&& handler);

    bool asyncRecv(
        nx::Buffer* buffer,
        std::function<void(SystemError::ErrorCode,std::size_t)>&& handler);

    void waitForAllPendingIOFinish()
    {
        m_underlySocket->pleaseStopSync();
        clear();
    }

    void clear()
    {
        m_readQueue.clear();
        m_writeQueue.clear();
    }

private:
    void asyncPerform(SslAsyncOperation* operation);

    // Perform the corresponding SSL operations that is outstanding here.
    // Since all the operation will be serialized inside of a single thread.
    // We can simply pass all the parameter all around as a function parameter.
    void perform(SslAsyncOperation* operation);

    // Check whether SSL has been shutdown or not for this situations.
    void checkShutdown(int sslReturn, int sslError);

    // Handle SSL internal error
    void handleSslError(int sslReturn, int sslError);

    // These 2 functions will issue the read/write operations directly to the underlying
    // socket. It will use context variable outstanding_read_/outstanding_write_ operations
    void enqueueRead(SslAsyncOperation* operation);
    void doRead();
    void doWrite();
    void enqueueWrite();
    void doHandshake(SslAsyncOperation* next_op);

    void onRecv(SystemError::ErrorCode errorCode, std::size_t transferred);
    void onSend(SystemError::ErrorCode errorCode, std::size_t transferred);
    void onConnect(SystemError::ErrorCode errorCode);

    void continueRead();
    void continueWrite();
protected:
    AbstractStreamSocket* socket()
    {
        return m_underlySocket;
    }

    void injectSnifferData(const nx::Buffer& buffer)
    {
        m_recvBuffer.append(buffer);
    }

    enum
    {
        HANDSHAKE_NOT_YET,
        HANDSHAKE_PENDING,
        HANDSHAKE_DONE
    };

    void setHandshakeStage(int handshake_stage)
    {
        m_handshakeStage = handshake_stage;
    }

private:
    // Operations
    std::unique_ptr<SslAsyncRead> m_read;
    std::unique_ptr<SslAsyncWrite> m_write;
    std::unique_ptr<SslAsyncHandshake> m_handshake;

    // Handshake
    std::list<SslAsyncOperation*> m_handshakeQueue;

    // Read
    bool m_allowBioRead;
    nx::Buffer m_recvBuffer;
    std::size_t m_recvBufferReadPos;
    std::list<SslAsyncOperation*> m_readQueue;
    SslAsyncOperation* m_outstandingRead;

    // Write
    struct PendingWrite
    {
        nx::Buffer writeBuffer;
        SslAsyncOperation* operation;

        PendingWrite(PendingWrite&& write)
        {
            writeBuffer = std::move(write.writeBuffer);
            operation = write.operation;
        }

        PendingWrite():operation(NULL){}

        bool needWrite() const
        {
            return !writeBuffer.isEmpty();
        }

        void reset(SslAsyncOperation* op) {
            operation = op;
            writeBuffer.clear();
        }
    };

    std::list<PendingWrite> m_writeQueue;
    PendingWrite* m_outstandingWrite;
    PendingWrite m_currentWrite;

    // SSL related
    SSL* m_ssl;
    bool m_eof;
    bool m_isServer;
    int m_handshakeStage;
    AbstractStreamSocket* m_underlySocket;

    // This class is used to notify the caller that the user has deleted the
    // object. Since it is very likely that the invocation will spawn a chain
    // of member function in AsyncSSL, eg: OnUnderlySocketRecv --> sendAsync
    // --> OnUnderlySocketSend --> delete the object, for each function, there
    // will be a such class on the stack, however, the inner most nested class
    // will get notification, since this class set the deletion_flag at last.
    // This will result in the outer caller not get any notification afterwards.
    // A simpler and elegant way should use std::shared_ptr/std::weak_ptr, however
    // since our code has performance issue, we'd like to invent some easier way
    // to do so. This class will cascade the deletion information to the out most
    // callee until no more DeletionFlag is used then. The mechanism is simple,it
    // will check the AsyncSSL's deletion_flag_ pointer, if it is not NULL, which
    // means a DelegionFlag on stack is watching for the deletion operations, so
    // it will cache this pointer and set it to the corresponding value, by this
    // means we are able to cascade the deletion operation internally.
    class DeletionFlag
    {
    public:
        DeletionFlag(SslAsyncBioHelper* ssl) :
            m_flag(false),
            m_ssl(ssl)
        {
            m_previousFlag = ssl->m_deletionFlag;
            ssl->m_deletionFlag = &m_flag;
        }
        ~DeletionFlag()
        {
            if (!m_flag) {
                // Restoring the previous flag of this objects
                m_ssl->m_deletionFlag = m_previousFlag;
            } else {
                if (m_previousFlag != NULL)
                    *m_previousFlag = true;
            }
        }

        operator bool () const
        {
            return m_flag;
        }

    private:
        bool m_flag;
        bool* m_previousFlag;
        SslAsyncBioHelper* m_ssl;
    };
    bool* m_deletionFlag;
    friend class DeletionFlag;
};

SslAsyncBioHelper::SslAsyncBioHelper(
        SSL* ssl, AbstractStreamSocket* underly_socket, bool is_server)
:
    m_read(new SslAsyncRead(ssl)),
    m_write(new SslAsyncWrite(ssl)),
    m_handshake(new SslAsyncHandshake(ssl)),
    m_allowBioRead(false),
    m_recvBufferReadPos(0),
    m_ssl(ssl),
    m_eof(false),
    m_isServer(is_server),
    m_handshakeStage(HANDSHAKE_NOT_YET),
    m_underlySocket(underly_socket),
    m_deletionFlag(NULL)
{
        m_recvBuffer.reserve(static_cast<int>(kSslAsyncRecvBufferSize));
}

SslAsyncBioHelper::~SslAsyncBioHelper()
{
    if (m_deletionFlag != NULL) {
        *m_deletionFlag = true;
    }
}

void SslAsyncBioHelper::perform(SslAsyncOperation* operation)
{
    int sslReturn, sslError ;
    // Setting up the write buffer here
    m_currentWrite.reset(operation);
    // Perform the underlying SSL operations
    operation->perform(&sslReturn,&sslError);
    // Check the EOF/SHUTDOWN status of the ssl
    checkShutdown(sslReturn,sslError);
    // Handle existed write here
    if (m_currentWrite.needWrite()) {
        enqueueWrite();
    }

    switch(sslError) {
    case SSL_ERROR_NONE:
        // The SSL operation has been finished here
        operation->setExitStatus(
                SslAsyncOperation::SUCCESS,SystemError::noError);
        break;
    case SSL_ERROR_WANT_READ:
        enqueueRead(operation);
        break;
    case SSL_ERROR_WANT_WRITE:
        break;
    default:
        handleSslError(sslReturn,sslError);
        if (eof() && sslError == SSL_ERROR_SYSCALL) {
            // For end of the file, we just tell them that we are done here
            operation->setExitStatus(
                SslAsyncOperation::END_OF_STREAM, SystemError::noError);
        } else {
            operation->setExitStatus(
                SslAsyncOperation::EXCEPTION, SystemError::connectionAbort);
        }
        break;
    }
}

void SslAsyncBioHelper::checkShutdown(int sslReturn, int sslError)
{
    Q_UNUSED(sslReturn);
    if (SSL_get_shutdown(m_ssl) == SSL_RECEIVED_SHUTDOWN ||
        sslError == SSL_ERROR_ZERO_RETURN) {
            // This should be the normal shutdown which means the
            // peer at least call SSL_shutdown once
            m_eof = true;
    } else if (sslError == SSL_ERROR_SYSCALL) {
        // Brute shutdown for SSL connection
        if (ERR_get_error() == 0) {
            m_eof = true;
        }
    }
}

const char* SSLErrorStr(int sslError)
{
    switch(sslError)
    {
        case SSL_ERROR_SSL: return "SSL_ERROR_SSL";
        case SSL_ERROR_SYSCALL: return "SSL_ERROR_SYSCALL";
        default: return "<ssl not a real error>";
    }
}

void SslAsyncBioHelper::handleSslError(int sslReturn, int sslError)
{
    QStringList errorStack;
    while (int err = ERR_get_error())
    {
        char err_str[1024];
        ERR_error_string_n(err, err_str, 1024);
        errorStack << QLatin1String(err_str);
    }

    NX_LOGX(lm("SSL returns %1, error %2, stack: %3")
        .arg(sslReturn).arg(sslError)
        .arg(errorStack.join(QLatin1String(", "))), cl_logDEBUG1);
}

void SslAsyncBioHelper::onRecv(
    SystemError::ErrorCode errorCode, std::size_t transferred)
{
    if (m_readQueue.empty()) return;
    if (errorCode != SystemError::noError) {
        DeletionFlag deleted(this);
        m_outstandingRead->setExitStatus(SslAsyncOperation::EXCEPTION,errorCode);
        if (!deleted) {
            m_outstandingRead->decreasePendingIOCount();
            if (!deleted)
                continueRead();
        }
        return;
    } else {
        if (transferred == 0) {
            m_eof = true;
        }
        // Since we gonna invoke user's callback here, a deletion flag will us
        // to avoid reuse member object once the user deleted such object
        DeletionFlag deleted(this);
        // Set up the flag to let the user runs into the read operation's returned buffer
        m_allowBioRead = true;
        perform(m_outstandingRead);
        if (!deleted) {
            m_outstandingRead->decreasePendingIOCount();
            if (!deleted) {
                m_allowBioRead = false;
                continueRead();
            }
        }
    }
}

void SslAsyncBioHelper::enqueueRead(SslAsyncOperation* operation)
{
    m_readQueue.push_back(operation);
    if (m_readQueue.size() ==1) {
        m_outstandingRead = m_readQueue.front();
        doRead();
    }
}

void SslAsyncBioHelper::continueRead()
{
    if (m_readQueue.empty()) return;
    m_readQueue.pop_front();
    if (!m_readQueue.empty()) {
        m_outstandingRead = m_readQueue.front();
        doRead();
    }
}

void SslAsyncBioHelper::doRead()
{
    // Setting up the outstanding read here
    m_outstandingRead = m_readQueue.front();
    // Checking if we have some data lefts inside of the read buffer, if so
    // we could just call SSL operation right here. And since this function
    // is executed inside of AIO thread, no recursive lock will happened in
    // user's callback function.
    if (static_cast<int>(m_recvBufferReadPos) < m_recvBuffer.size()) {
        m_outstandingRead->increasePendingIOCount();
        onRecv(SystemError::noError,
            m_recvBuffer.size() - m_recvBufferReadPos);
    } else {
        // We have to issue our operations right just here since we have no
        // left data inside of the buffer and our user needs to read all the
        // data out in the buffer here.
        m_underlySocket->readSomeAsync(
            &m_recvBuffer,
            std::bind(
            &SslAsyncBioHelper::onRecv,
            this,
            std::placeholders::_1,
            std::placeholders::_2));
        m_outstandingRead->increasePendingIOCount();
    }
}

void SslAsyncBioHelper::onSend(
        SystemError::ErrorCode errorCode,
        std::size_t transferred)
{
    Q_UNUSED(transferred);
    if (m_writeQueue.empty()) return;
    if (errorCode != SystemError::noError) {
        DeletionFlag deleted(this);
        m_outstandingWrite->operation->setExitStatus(
                SslAsyncOperation::EXCEPTION, errorCode);
        if (!deleted) {
            m_outstandingWrite->operation->decreasePendingIOCount();
            if (!deleted)
                continueWrite();
        }
        return;
    } else {
        // For read operations, if it succeeded, we don't need to call Perform
        // since this operation has been go ahead for that SSL_related operations.
        // What we need to do is just decrease user's pending IO count value.
        DeletionFlag deleted(this);
        m_outstandingWrite->operation->decreasePendingIOCount();
        if (!deleted) {
            continueWrite();
        }
    }
}

void SslAsyncBioHelper::doWrite()
{
    m_underlySocket->sendAsync(
        m_outstandingWrite->writeBuffer,
        std::bind(
        &SslAsyncBioHelper::onSend,
        this,
        std::placeholders::_1,
        std::placeholders::_2));
    m_outstandingWrite->operation->increasePendingIOCount();
}

void SslAsyncBioHelper::enqueueWrite()
{
    m_writeQueue.push_back(std::move(m_currentWrite));
    if (m_writeQueue.size() == 1) {
        m_outstandingWrite = &(m_writeQueue.front());
        doWrite();
    }
}

void SslAsyncBioHelper::continueWrite()
{
    if (m_writeQueue.empty()) return;
    m_writeQueue.pop_front();
    if (!m_writeQueue.empty()) {
        m_outstandingWrite = &(m_writeQueue.front());
        doWrite();
    }
}

std::size_t SslAsyncBioHelper::bioRead(void* data, std::size_t size)
{
    if (m_allowBioRead) {
        if (m_recvBuffer.size() == static_cast<int>(m_recvBufferReadPos)) {
            return 0;
        } else {
            std::size_t digest_size =
                std::min(size,
                    static_cast<int>(m_recvBuffer.size()) - m_recvBufferReadPos);
            memcpy(data,
                   m_recvBuffer.constData() + m_recvBufferReadPos,
                   digest_size);
            m_recvBufferReadPos += digest_size;
            if (static_cast<int>(m_recvBufferReadPos) == m_recvBuffer.size()) {
                m_recvBufferReadPos = 0;
                m_recvBuffer.clear();
                m_recvBuffer.reserve(static_cast<int>(kSslAsyncRecvBufferSize));
            }
            return digest_size;
        }
    }
    return 0;
}

std::size_t SslAsyncBioHelper::bioWrite(const void* data, std::size_t size)
{
    m_currentWrite.writeBuffer.append(
        static_cast<const char*>(data), static_cast<int>(size));

    return size;
}

void SslAsyncBioHelper::onConnect(SystemError::ErrorCode errorCode)
{
    if (errorCode != SystemError::noError) {
        m_handshakeStage = HANDSHAKE_NOT_YET;
        // No, we cannot connect to the peer sides, so we just tell our user
        // that we have expired whatever we've got currently
        for (auto op : m_handshakeQueue) {
            DeletionFlag deleted(this);
            op->setExitStatus(SslAsyncOperation::EXCEPTION, errorCode);
            if (deleted)
                return;
        }
        m_handshakeQueue.clear();
    } else {
        m_handshakeStage = HANDSHAKE_DONE;
        for (auto op : m_handshakeQueue) {
            DeletionFlag deleted(this);
            perform(op);
            if (deleted)
                return;
        }
        m_handshakeQueue.clear();
    }
}

void SslAsyncBioHelper::doHandshake(SslAsyncOperation* next_op)
{
    m_handshakeQueue.push_back(next_op);
    if (m_isServer) {
        SSL_set_accept_state(m_ssl);
    } else {
        SSL_set_connect_state(m_ssl);
    }
    m_handshake->reset(
        std::bind(
        &SslAsyncBioHelper::onConnect,
        this,
        std::placeholders::_1));
    perform(m_handshake.get());
}

void SslAsyncBioHelper::asyncPerform(SslAsyncOperation* operation)
{
    switch(m_handshakeStage) {
        case HANDSHAKE_NOT_YET:
            m_handshakeStage = HANDSHAKE_PENDING;
            doHandshake(operation);
            break;
        case HANDSHAKE_PENDING:
            m_handshakeQueue.push_back(operation);
            break;
        case HANDSHAKE_DONE:
            perform(operation);
            break;
        default:
            NX_ASSERT(0);
            break;
    }
}

bool SslAsyncBioHelper::asyncSend(
    const nx::Buffer& buffer,
    std::function<void(SystemError::ErrorCode,std::size_t)>&& handler)
{
    NX_ASSERT(m_handshakeStage == HANDSHAKE_DONE || !m_isServer,
        "SSL server is not supposed to send before recv");

    m_write->reset(&buffer,std::move(handler));
    asyncPerform(m_write.get());
    return true;
}

bool SslAsyncBioHelper::asyncRecv(
    nx::Buffer* buffer,
    std::function<void(SystemError::ErrorCode,std::size_t)>&& handler)
{
    m_read->reset(buffer,std::move(handler));
    asyncPerform(m_read.get());
    return true;
}

class MixedSslAsyncBioHelper : public SslAsyncBioHelper
{
public:
    struct SnifferData
    {
        std::function<void(SystemError::ErrorCode,std::size_t)> completionHandler;
        nx::Buffer* buffer;

        SnifferData(
            std::function<void(SystemError::ErrorCode,std::size_t)>&& completionHandler,
            nx::Buffer* buf)
        :
            completionHandler(completionHandler),
            buffer(buf)
        {}
    };

public:
    static const int kSnifferDataHeaderLength = 2;

    MixedSslAsyncBioHelper(SSL* ssl, AbstractStreamSocket* socket)
        :SslAsyncBioHelper(ssl,socket,true),
        m_isInitialized(false),
        m_isSsl(false)
    {}

    // User could not issue a async_send before a async_recv . Otherwise
    // we are in trouble since we don't know the type of the underly socking
    void asyncSend(
        const nx::Buffer& buffer,
        std::function<void(SystemError::ErrorCode,std::size_t)>&& op)
    {
            // When you see this, it means you screw up since the very first call should
            // be a async_recv instead of async_send here .
            NX_ASSERT(m_isInitialized);
            NX_ASSERT(m_isSsl);
            SslAsyncBioHelper::asyncSend(buffer,std::move(op));
    }

    void asyncRecv(
        nx::Buffer* buffer,
        std::function<void(SystemError::ErrorCode,std::size_t)>&& completionHandler)
    {
        if (!m_isInitialized) {
            // We need to sniffer the buffer here
            m_snifferBuffer.reserve(kSnifferDataHeaderLength);
            socket()->readSomeAsync(
                &m_snifferBuffer,
                std::bind(
                    &MixedSslAsyncBioHelper::Sniffer,
                    this,
                    std::placeholders::_1,
                    std::placeholders::_2,
                    SnifferData(std::move(completionHandler),buffer)));
        } else {
            NX_ASSERT(m_isSsl);
            SslAsyncBioHelper::asyncRecv(buffer,std::move(completionHandler));
        }
    }

    // When blocking version detects it is an SSL, it has to notify me
    void set_ssl(bool ssl)
    {
        m_isInitialized = true;
        m_isSsl = ssl;
        SslAsyncBioHelper::setHandshakeStage(SslAsyncBioHelper::HANDSHAKE_DONE);
    }

    bool is_initialized() const
    {
        return m_isInitialized;
    }

    bool is_ssl() const
    {
        return m_isSsl;
    }

private:
    void Sniffer(
        SystemError::ErrorCode ec,
        std::size_t transferred,
        SnifferData data)
    {
        // We have the data in our buffer right now
        if (ec) {
            data.completionHandler(ec,0);
            return;
        } else {
            if (transferred == 0) {
                data.completionHandler(ec,0);
                return;
            } else if (m_snifferBuffer.size() < kSnifferDataHeaderLength) {
                socket()->readSomeAsync(
                    &m_snifferBuffer,
                    std::bind(
                    &MixedSslAsyncBioHelper::Sniffer,
                    this,
                    std::placeholders::_1,
                    std::placeholders::_2,
                    data));
                return;
            }
            // Fix for the bug that always false in terms of comparison of 0x80
            const unsigned char* buf = reinterpret_cast<unsigned char*>(
                m_snifferBuffer.data());

            if (buf[0] == 0x80 || (buf[0] == 0x16 && buf[1] == 0x03)) {
                m_isSsl = true;
                m_isInitialized = true;
            } else {
                m_isSsl = false;
                m_isInitialized = true;
            }

            // If we are SSL, we need to push the data into the buffer
            SslAsyncBioHelper::injectSnifferData(m_snifferBuffer);

            // If it is an SSL we still need to continue our async operation
            // otherwise we call the failed callback for the upper usage class
            // it should be MixedSslSocket class
            if (m_isSsl) {
                // request a SSL async recv
                SslAsyncBioHelper::asyncRecv(
                    data.buffer, std::move(data.completionHandler));
            } else {
                // request a common async recv
                data.buffer->append(m_snifferBuffer);
                data.completionHandler(SystemError::noError, data.buffer->size());
            }
        }
    }

private:
    bool m_isInitialized;
    bool m_isSsl;
    nx::Buffer m_snifferBuffer;

};

class SslStaticData
{
public:
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey;
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> serverContext;
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> clientContext;

    SslStaticData()
    :
        pkey(nullptr, &EVP_PKEY_free),
        serverContext(nullptr, &SSL_CTX_free),
        clientContext(nullptr, &SSL_CTX_free)
    {
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();

        serverContext.reset(SSL_CTX_new(SSLv23_server_method()));
        clientContext.reset(SSL_CTX_new(SSLv23_client_method()));

        SSL_CTX_set_options(serverContext.get(), SSL_OP_SINGLE_DH_USE);
        SSL_CTX_set_session_id_context(
            serverContext.get(),
            reinterpret_cast<const unsigned char*>(kSslSessionId.data()),
            kSslSessionId.size());
    }

    static SslStaticData* instance();
};

Q_GLOBAL_STATIC(SslStaticData, SslStaticData_instance);

SslStaticData* SslStaticData::instance()
{
    return SslStaticData_instance();
}

const size_t SslEngine::kBufferSize = 1024 * 10;
const int SslEngine::kRsaLength = 2048;
const std::chrono::seconds SslEngine::kCertExpiration =
    std::chrono::hours(5 * 365 * 24); // 5 years

String SslEngine::makeCertificateAndKey(
    const String& common, const String& country, const String& company)
{
    const auto data = SslStaticData::instance();
    const int serialNumber = qrand();

    auto number = utils::wrapUnique(BN_new(), &BN_free);
    if (!number || !BN_set_word(number.get(), RSA_F4))
    {
        NX_LOG("SSL cant generate big number", cl_logWARNING);
        return String();
    }

    auto rsa = utils::wrapUnique(RSA_new(), &RSA_free);
    if (!rsa || !RSA_generate_key_ex(rsa.get(), kRsaLength, number.get(), NULL))
    {
        NX_LOG("SSL cant generate RSA", cl_logWARNING);
        return String();
    }

    auto pkey = utils::wrapUnique(EVP_PKEY_new(), &EVP_PKEY_free);
    if (!pkey || !EVP_PKEY_assign_RSA(pkey.get(), rsa.release()))
    {
        NX_LOG("SSL cant generate PKEY", cl_logWARNING);
        return String();
    }

    auto x509 = utils::wrapUnique(X509_new(), &X509_free);
    if (!x509
        || !ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), serialNumber)
        || !X509_gmtime_adj(X509_get_notBefore(x509.get()), 0)
        || !X509_gmtime_adj(X509_get_notAfter(x509.get()), kCertExpiration.count())
        || !X509_set_pubkey(x509.get(), pkey.get()))
    {
        NX_LOG("SSL cant generate X509 cert", cl_logWARNING);
        return String();
    }

    const auto name = X509_get_subject_name(x509.get());
    const auto nameSet = [&name](const char* field, const String& value)
    {
        auto vptr = (unsigned char *)value.data();
        return X509_NAME_add_entry_by_txt(
            name, field,  MBSTRING_UTF8, vptr, -1, -1, 0);
    };

    if (!name
        || !nameSet("C", country) || !nameSet("O", company) || !nameSet("CN", common)
        || !X509_set_issuer_name(x509.get(), name)
        || !X509_sign(x509.get(), pkey.get(), EVP_sha1()))
    {
        NX_LOG("SSL cant sign X509 cert", cl_logWARNING);
        return String();
    }

    char writeBuffer[kBufferSize] = { 0 };
    const auto bio = utils::wrapUnique(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio
        || !PEM_write_bio_PrivateKey(bio.get(), pkey.get(), 0, 0, 0, 0, 0)
        || !PEM_write_bio_X509(bio.get(), x509.get())
        || !BIO_read(bio.get(), writeBuffer, kBufferSize))
    {
        NX_LOG("SSL cant generate cert string", cl_logWARNING);
        return String();
    }

    return String(writeBuffer);
}

bool SslEngine::useCertificateAndPkey(const String& certData)
{
    Buffer certBytes(certData);
    const auto data = SslStaticData::instance();
    {
        auto bio = utils::wrapUnique(
            BIO_new_mem_buf(static_cast<void*>(certBytes.data()), certBytes.size()),
            &BIO_free);

        auto x509 = utils::wrapUnique(
            PEM_read_bio_X509_AUX(bio.get(), 0, 0, 0), &X509_free);

        if (!x509)
        {
            NX_LOG("SSL cannot read X509", cl_logDEBUG1);
            return false;
        }

        if (!SSL_CTX_use_certificate(data->serverContext.get(), x509.get()))
        {
            NX_LOG("SSL cannot use X509", cl_logWARNING);
            return false;
        }
    }

    {
        auto bio = utils::wrapUnique(
            BIO_new_mem_buf(static_cast<void*>(certBytes.data()), certBytes.size()),
            &BIO_free);

        data->pkey.reset(PEM_read_bio_PrivateKey(bio.get(), 0, 0, 0));
        if (!data->pkey)
        {
            NX_LOG("SSL cannot read PKEY", cl_logDEBUG1);
            return false;
        }

        if (!SSL_CTX_use_PrivateKey(data->serverContext.get(), data->pkey.get()))
        {
            NX_LOG("SSL cannot use PKEY", cl_logWARNING);
            return false;
        }
    }

    return true;
}

class SslSocketPrivate
{
public:
    AbstractStreamSocket* wrappedSocket;
    std::unique_ptr<SSL, decltype(&SSL_free)> ssl;
    bool isServerSide;
    quint8 extraBuffer[32];
    int extraBufferLen;
    //!Socket works as regular socket (without encryption until this flag is set to \a true)
    bool ecnryptionEnabled;

    // This model flag will be set up by the interface internally. It just tells
    // the user what our socket will be. An async version or a sync version. We
    // keep the sync mode for historic reason, but during the support for async,
    // the call for sync is undefined. This is for purpose since it heavily reduce
    // the pain of
    std::atomic<SslSocket::IOMode> ioMode;
    std::atomic<bool> emulateBlockingMode;
    std::unique_ptr<SslAsyncBioHelper> asyncSslHelper;

    typedef utils::promise<std::pair<SystemError::ErrorCode, size_t>> AsyncPromise;
    std::unique_ptr<AsyncPromise> sendPromise;
    std::unique_ptr<AsyncPromise> recvPromise;

    SslSocketPrivate()
    :
        wrappedSocket(nullptr),
        ssl(nullptr, SSL_free),
        isServerSide(false),
        extraBufferLen(0),
        ecnryptionEnabled(false),
        ioMode(SslSocket::SYNC),
        emulateBlockingMode(false)
    {
    }
};

SslSocket::SslSocket(
    AbstractStreamSocket* wrappedSocket,
    bool isServerSide, bool encriptionEnforced)
:
    base_type([wrappedSocket](){ return wrappedSocket; }),
    d_ptr(new SslSocketPrivate())
{
    Q_D(SslSocket);
    d->wrappedSocket = wrappedSocket;
    d->isServerSide = isServerSide;
    d->extraBufferLen = 0;
    d->ecnryptionEnabled = encriptionEnforced;
    init();
    initOpenSSLGlobalLock();
    d->asyncSslHelper.reset(new SslAsyncBioHelper(
        d->ssl.get(), d->wrappedSocket,isServerSide));
}

SslSocket::SslSocket(
    SslSocketPrivate* priv, AbstractStreamSocket* wrappedSocket,
    bool isServerSide, bool encriptionEnforced)
:
    base_type([wrappedSocket](){ return wrappedSocket; }),
    d_ptr(priv)
{
    Q_D(SslSocket);
    d->wrappedSocket = wrappedSocket;
    d->isServerSide = isServerSide;
    d->extraBufferLen = 0;
    d->ecnryptionEnabled = encriptionEnforced;
    init();
    initOpenSSLGlobalLock();
}

void SslSocket::init()
{
    Q_D(SslSocket);
    static BIO_METHOD kBioMethods =
    {
        BIO_TYPE_SOCKET,
        "proxy server socket",
        bioWrite,
        bioRead,
        bioPuts,
        NULL, // bgets
        bioCtrl,
        bioNew,
        bioFree,
        NULL, // callback_ctrl
    };

    BIO* rbio = BIO_new(&kBioMethods);
    BIO_set_nbio(rbio, 1);
    BIO_set_app_data(rbio, this);

    BIO* wbio = BIO_new(&kBioMethods);
    BIO_set_app_data(wbio, this);
    BIO_set_nbio(wbio, 1);

    auto context = d->isServerSide
        ? SslStaticData::instance()->serverContext.get()
        : SslStaticData::instance()->clientContext.get();

    NX_ASSERT(context);
    d->ssl.reset(SSL_new(context)); // get new SSL state with context

    SSL_set_verify(d->ssl.get(), SSL_VERIFY_NONE, NULL);
    SSL_set_session_id_context(
        d->ssl.get(),
        reinterpret_cast<const unsigned char*>(kSslSessionId.data()),
        kSslSessionId.size());

    SSL_set_bio(d->ssl.get(), rbio, wbio);  //d->ssl will free bio when freed
}

int SslSocket::bioRead(BIO* b, char* out, int outl)
{
    SslSocket* sslSock = static_cast<SslSocket*>(BIO_get_app_data(b));
    if (sslSock->ioMode() == SslSocket::ASYNC)
    {
        int ret = sslSock->asyncRecvInternal(out, outl);
        if (ret == -1)
        {
            BIO_clear_retry_flags(b);
            BIO_set_retry_read(b);
        }

        return ret;
    }

    int ret = 0;
    if (out != NULL)
    {
        //clear_socket_error();
        ret = sslSock->recvInternal(out, outl, 0);
        BIO_clear_retry_flags(b);
        if (ret <= 0)
        {
            const int sysErrorCode = SystemError::getLastOSErrorCode();
            if (sysErrorCode == SystemError::wouldBlock ||
                sysErrorCode == SystemError::again ||
                BIO_sock_should_retry(sysErrorCode))
            {
                BIO_set_retry_read(b);
            }
        }
    }

    return ret;
}

int SslSocket::bioWrite(BIO* b, const char* in, int inl)
{
    SslSocket* sslSock = static_cast<SslSocket*>(BIO_get_app_data(b));
    if (sslSock->ioMode() == SslSocket::ASYNC)
    {
        int ret = sslSock->asyncSendInternal(in, inl);
        if (ret == -1)
        {
            BIO_clear_retry_flags(b);
            BIO_set_retry_write(b);
        }

        return ret;
    }

    //clear_socket_error();
    int ret = sslSock->sendInternal(in, inl);
    BIO_clear_retry_flags(b);
    if (ret <= 0)
    {
        const int sysErrorCode = SystemError::getLastOSErrorCode();
        if (sysErrorCode == SystemError::wouldBlock ||
            sysErrorCode == SystemError::again ||
            BIO_sock_should_retry(sysErrorCode))
        {
            BIO_set_retry_write(b);
        }
    }

    return ret;
}

int SslSocket::bioPuts(BIO* bio, const char* str)
{
    return bioWrite(bio, str, strlen(str));
}

long SslSocket::bioCtrl(BIO* bio, int cmd, long num, void* /*ptr*/)
{
    long ret = 1;

    switch (cmd)
    {
        case BIO_C_SET_FD:
            NX_ASSERT("Invalid proxy socket use!");
            break;
        case BIO_C_GET_FD:
            NX_ASSERT("Invalid proxy socket use!");
            break;
        case BIO_CTRL_GET_CLOSE:
            ret = bio->shutdown;
            break;
        case BIO_CTRL_SET_CLOSE:
            bio->shutdown = (int)num;
            break;
        case BIO_CTRL_DUP:
        case BIO_CTRL_FLUSH:
            ret = 1;
            break;
        default:
            ret = 0;
            break;
    }
    return(ret);
}

int SslSocket::bioNew(BIO* bio)
{
    bio->init = 1;
    bio->num = 0;
    bio->ptr = NULL;
    bio->flags = 0;
    return 1;
}

int SslSocket::bioFree(BIO* bio)
{
    if (bio == NULL) return(0);
    if (bio->shutdown)
    {
        if (bio->init)
        {
            SslSocket* sslSock = static_cast<SslSocket*>(BIO_get_app_data(bio));
            if (sslSock)
                sslSock->close();
        }

        bio->init = 0;
        bio->flags = 0;
    }

    return 1;
}

SslSocket::~SslSocket()
{
    Q_D(SslSocket);
    if (d->ioMode == ASYNC && d->asyncSslHelper)
            d->asyncSslHelper->waitForAllPendingIOFinish();

    delete d->wrappedSocket;
    delete d_ptr;
}


bool SslSocket::doHandshake()
{
    Q_D(SslSocket);
    if (d->isServerSide)
        SSL_set_accept_state(d->ssl.get());
    else
        SSL_set_connect_state(d->ssl.get());

    int ret = SSL_do_handshake(d->ssl.get());
    if (ret != 1)
    {
        QByteArray e(1024, '\0');
        ERR_error_string_n(SSL_get_error(d->ssl.get(), ret), e.data(), e.size());
        NX_LOGX(lm("handshake (isServer=%1) failed %2: %3")
            .arg(d->isServerSide).arg(ret).arg(e), cl_logDEBUG1);
    }
    else
    {
        NX_LOGX(lm("handshake (isServer=%1) success %2")
            .arg(d->isServerSide).arg(ret), cl_logDEBUG2);
    }

    return ret == 1;
}

int SslSocket::recvInternal(void* buffer, unsigned int bufferLen, int /*flags*/)
{
    Q_D(SslSocket);
    if (d->extraBufferLen > 0)
    {
        int toReadLen = qMin((int)bufferLen, d->extraBufferLen);
        memcpy(buffer, d->extraBuffer, toReadLen);
        memmove(d->extraBuffer, d->extraBuffer + toReadLen, d->extraBufferLen - toReadLen);
        d->extraBufferLen -= toReadLen;
        int readRest = bufferLen - toReadLen;
        // Does this should be readreset ? -- DPENG
        if (toReadLen > 0) {
            int readed = d->wrappedSocket->recv((char*) buffer + toReadLen, readRest);
            if (readed > 0)
                toReadLen += readed;
        }
        return toReadLen;
    }
    return d->wrappedSocket->recv(buffer, bufferLen);
}

int SslSocket::recv(void* buffer, unsigned int bufferLen, int flags)
{
    Q_D(SslSocket);
    if (d->emulateBlockingMode)
    {
        // the mode could be switched to non blocking, but SSL engine is
        // still non-blocking
        d->recvPromise.reset(new SslSocketPrivate::AsyncPromise);
        nx::Buffer localBuffer(bufferLen, '\0');
        readSomeAsync(
            &localBuffer,
            [&](SystemError::ErrorCode code, size_t size)
            {
                d->recvPromise->set_value(std::make_pair(code, size));
                d->recvPromise.reset();
            });

        const auto result = d->recvPromise->get_future().get();
        if (result.first == SystemError::noError)
            std::memcpy(localBuffer.data(), buffer, result.second);
        else
            SystemError::setLastErrorCode(result.first);

        return result.second;
    }

    NX_ASSERT(d->ioMode == SslSocket::SYNC);
    if (!d->ecnryptionEnabled)
        return d->wrappedSocket->recv(buffer, bufferLen, flags);

    if (!SSL_is_init_finished(d->ssl.get()))
        doHandshake();

    return SSL_read(d->ssl.get(), (char*) buffer, bufferLen);
}

int SslSocket::sendInternal(const void* buffer, unsigned int bufferLen)
{
    Q_D(SslSocket);
    return d->wrappedSocket->send(buffer,bufferLen);
}

int SslSocket::send(const void* buffer, unsigned int bufferLen)
{
    Q_D(SslSocket);
    if (d->emulateBlockingMode)
    {
        // the mode could be switched to non blocking, but SSL engine is
        // still non-blocking
        d->sendPromise.reset(new SslSocketPrivate::AsyncPromise);
        nx::Buffer localBuffer(static_cast<const char*>(buffer), bufferLen);
        sendAsync(
            localBuffer,
            [&](SystemError::ErrorCode code, size_t size)
            {
                d->sendPromise->set_value(std::make_pair(code, size));
                d->sendPromise.reset();
            });

        const auto result = d->sendPromise->get_future().get();
        if (result.first != SystemError::noError)
            SystemError::setLastErrorCode(result.first);

        return result.second;
    }

    NX_ASSERT(d->ioMode == SslSocket::SYNC);
    if (!d->ecnryptionEnabled)
        return d->wrappedSocket->send(buffer, bufferLen);

    if (!SSL_is_init_finished(d->ssl.get()))
        doHandshake();

    return SSL_write(d->ssl.get(), buffer, bufferLen);
}

bool SslSocket::reopen()
{
    Q_D(SslSocket);
    d->ecnryptionEnabled = false;
    return d->wrappedSocket->reopen();
}

bool SslSocket::setNoDelay(bool value)
{
    Q_D(SslSocket);
    return d->wrappedSocket->setNoDelay(value);
}

bool SslSocket::getNoDelay(bool* value) const
{
    Q_D(const SslSocket);
    return d->wrappedSocket->getNoDelay(value);
}

bool SslSocket::toggleStatisticsCollection(bool val)
{
    Q_D(SslSocket);
    return d->wrappedSocket->toggleStatisticsCollection(val);
}

bool SslSocket::getConnectionStatistics(StreamSocketInfo* info)
{
    Q_D(SslSocket);
    return d->wrappedSocket->getConnectionStatistics(info);
}

bool SslSocket::connect(
    const SocketAddress& remoteAddress,
    unsigned int timeoutMillis)
{
    Q_D(SslSocket);
    d->ecnryptionEnabled = true;
    return d->wrappedSocket->connect(remoteAddress, timeoutMillis);
}

SocketAddress SslSocket::getForeignAddress() const
{
    Q_D(const SslSocket);
    return d->wrappedSocket->getForeignAddress();
}

bool SslSocket::isConnected() const
{
    Q_D(const SslSocket);
    return d->wrappedSocket->isConnected();
}

bool SslSocket::setKeepAlive(boost::optional< KeepAliveOptions > info)
{
    Q_D(const SslSocket);
    return d->wrappedSocket->setKeepAlive(info);
}

bool SslSocket::getKeepAlive(boost::optional< KeepAliveOptions >* result) const
{
    Q_D(const SslSocket);
    return d->wrappedSocket->getKeepAlive(result);
}

bool SslSocket::connectWithoutEncryption(
    const QString& foreignAddress,
    unsigned short foreignPort,
    unsigned int timeoutMillis)
{
    Q_D(const SslSocket);
    return d->wrappedSocket->connect(foreignAddress, foreignPort, timeoutMillis);
}

bool SslSocket::enableClientEncryption()
{
    Q_D(SslSocket);
    NX_ASSERT(!d->isServerSide);
    d->ecnryptionEnabled = true;
    return doHandshake();
}

void SslSocket::cancelIOAsync(
    nx::network::aio::EventType eventType,
    nx::utils::MoveOnlyFunc<void()> cancellationDoneHandler)
{
    Q_D(const SslSocket);
    d->wrappedSocket->cancelIOAsync(
        eventType,
        [cancellationDoneHandler = move(cancellationDoneHandler), d](){
            d->asyncSslHelper->clear();
            cancellationDoneHandler();
        });
}

void SslSocket::cancelIOSync(nx::network::aio::EventType eventType)
{
    Q_D(const SslSocket);
    d->wrappedSocket->cancelIOSync(eventType);
}

bool SslSocket::setNonBlockingMode(bool val)
{
    Q_D(SslSocket);
    if (d->ioMode == SslSocket::SYNC)
        return d->wrappedSocket->setNonBlockingMode(val);

    // the mode could be switched to non blocking, but SSL engine is
    // too heavy to switch
    d->emulateBlockingMode = !val;
    return true;
}

bool SslSocket::getNonBlockingMode(bool* val) const
{
    Q_D(const SslSocket);
    if (d->ioMode == SslSocket::SYNC)
        return d->wrappedSocket->getNonBlockingMode(val);

    *val = !d->emulateBlockingMode;
    return true;
}

bool SslSocket::shutdown()
{
    Q_D(const SslSocket);
    if (!d->emulateBlockingMode)
    {
        d->wrappedSocket->pleaseStopSync();
        return d->wrappedSocket->shutdown();
    }

    utils::promise<void> promise;
    d->wrappedSocket->dispatch([&]()
    {
        if (d->sendPromise)
            d->sendPromise->set_value({0, SystemError::interrupted});

        if (d->recvPromise)
            d->recvPromise->set_value({0, SystemError::interrupted});

        d->wrappedSocket->pleaseStopSync();
        promise.set_value();
    });

    promise.get_future().wait();
    return d->wrappedSocket->shutdown();
}

void SslSocket::connectAsync(
    const SocketAddress& addr,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    Q_D(const SslSocket);
    return d->wrappedSocket->connectAsync(addr, std::move(handler));
}

void SslSocket::readSomeAsync(
    nx::Buffer* const buffer,
    std::function<void(SystemError::ErrorCode, std::size_t)> handler)
{
    Q_D(SslSocket);
    d->wrappedSocket->post(
        [this,buffer,handler]() mutable {
            Q_D(SslSocket);
            d->ioMode.store(SslSocket::ASYNC,std::memory_order_release);
            d->asyncSslHelper->asyncRecv(buffer, std::move(handler));
        });
}

void SslSocket::sendAsync(
    const nx::Buffer& buffer,
    std::function<void(SystemError::ErrorCode, std::size_t)> handler)
{
    Q_D(SslSocket);
    d->wrappedSocket->post(
        [this,&buffer,handler]() mutable {
            Q_D(SslSocket);
            d->ioMode.store(SslSocket::ASYNC,std::memory_order_release);
            d->asyncSslHelper->asyncSend(buffer, std::move(handler));
    });
}

int SslSocket::asyncRecvInternal(void* buffer, unsigned int bufferLen) {
    // For async operation here
    Q_D(SslSocket);
    NX_ASSERT(ioMode() == ASYNC);
    NX_ASSERT(d->asyncSslHelper != NULL);
    int ret = d->asyncSslHelper->eof() ? 0 :
        static_cast<int>(d->asyncSslHelper->bioRead(buffer, bufferLen));

    #ifdef DEBUG_SSL
        NX_LOGX(lm("BIO read %1 returned %2").arg(bufferLen).arg(ret), cl_logDEBUG2);
    #endif
    return ret == 0 ? -1 : ret;
}

int SslSocket::asyncSendInternal(
    const void* buffer,
    unsigned int bufferLen)
{
    Q_D(SslSocket);
    NX_ASSERT(ioMode() == ASYNC);
    NX_ASSERT(d->asyncSslHelper != NULL);
    auto ret = static_cast<int>(d->asyncSslHelper->bioWrite(buffer, bufferLen));

    #ifdef DEBUG_SSL
        NX_LOGX(lm("BIO write %1 returned %2").arg(bufferLen).arg(ret), cl_logDEBUG2);
    #endif
    return ret;
}

void SslSocket::registerTimer(
    std::chrono::milliseconds timeoutMs,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    Q_D(SslSocket);
    return d->wrappedSocket->registerTimer(timeoutMs, std::move(handler));
}

SslSocket::IOMode SslSocket::ioMode() const {
    Q_D(const SslSocket);
    return d->ioMode.load(std::memory_order_acquire);
}

class MixedSslSocketPrivate: public SslSocketPrivate
{
public:
    bool initState;
    bool useSSL;

    MixedSslSocketPrivate()
    :
        initState(true),
        useSSL(false)
    {
    }
};

MixedSslSocket::MixedSslSocket(AbstractStreamSocket* wrappedSocket)
:
    SslSocket(new MixedSslSocketPrivate, wrappedSocket, true, false)
{
    Q_D(MixedSslSocket);
    d->initState = true;
    d->useSSL = false;
    d->asyncSslHelper.reset(new MixedSslAsyncBioHelper(
        d->ssl.get(), d->wrappedSocket));
}

int MixedSslSocket::recv(void* buffer, unsigned int bufferLen, int flags)
{
    Q_D(MixedSslSocket);
    NX_ASSERT(d->ioMode == SslSocket::SYNC);
    // check for SSL pattern 0x80 (v2) or 0x16 03 (v3)
    if (d->initState)
    {
        if (d->extraBufferLen == 0) {
            int readed = d->wrappedSocket->recv(d->extraBuffer, 1);
            if (readed < 1)
                return readed;
            d->extraBufferLen += readed;
        }

        if (d->extraBuffer[0] == 0x80)
        {
            d->useSSL = true;
            d->ecnryptionEnabled = true;
            d->initState = false;
        }
        else if (d->extraBuffer[0] == 0x16)
        {
            int readed = d->wrappedSocket->recv(d->extraBuffer+1, 1);
            if (readed < 1)
                return readed;
            d->extraBufferLen += readed;

            if (d->extraBuffer[1] == 0x03)
            {
                d->useSSL = true;
                d->ecnryptionEnabled = true;
            }
        }
        d->initState = false;
    }

    if (d->useSSL)
        return SslSocket::recv((char*) buffer, bufferLen, flags);
    else
        return recvInternal(buffer, bufferLen, flags);
}

int MixedSslSocket::send(const void* buffer, unsigned int bufferLen)
{
    Q_D(MixedSslSocket);
    NX_ASSERT(d->ioMode == SslSocket::SYNC);
    if (d->useSSL)
        return SslSocket::send((char*) buffer, bufferLen);
    else
        return d->wrappedSocket->send(buffer, bufferLen);
}

void MixedSslSocket::cancelIOAsync(
    nx::network::aio::EventType eventType,
    nx::utils::MoveOnlyFunc<void()> cancellationDoneHandler)
{
    Q_D(MixedSslSocket);
    if (d->useSSL)
        SslSocket::cancelIOAsync(eventType, std::move(cancellationDoneHandler));
    else
        d->wrappedSocket->cancelIOAsync(eventType, std::move(cancellationDoneHandler));
}

void MixedSslSocket::connectAsync(
    const SocketAddress& addr,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    Q_D(MixedSslSocket);
    if (d->useSSL)
        return SslSocket::connectAsync(addr, std::move(handler));
    else
        return d->wrappedSocket->connectAsync(addr, std::move(handler));
}

void MixedSslSocket::readSomeAsync(
    nx::Buffer* const buffer,
    std::function<void(SystemError::ErrorCode, std::size_t)> handler)
{
    Q_D(MixedSslSocket);
    if (!d->initState && !d->useSSL)
        return d->wrappedSocket->readSomeAsync(buffer, std::move(handler));

    auto helper = static_cast<MixedSslAsyncBioHelper*>(d->asyncSslHelper.get());
    d->ioMode.store(SslSocket::ASYNC,std::memory_order_release);
    if (helper->is_initialized() && !helper->is_ssl())
        return d->wrappedSocket->readSomeAsync(buffer,std::move(handler));

    d->wrappedSocket->dispatch(
        [d, helper, buffer, handler = std::move(handler)]() mutable
        {
            if (!d->initState)
                helper->set_ssl(d->useSSL);

            if (helper->is_initialized() && !helper->is_ssl())
                return d->wrappedSocket->readSomeAsync(buffer, std::move(handler));

            d->wrappedSocket->post(
                [helper, buffer, handler = std::move(handler)]() mutable
                {
                    helper->asyncRecv(buffer, std::move(handler));
                });
        });
}

void MixedSslSocket::sendAsync(
    const nx::Buffer& buffer,
    std::function<void(SystemError::ErrorCode, std::size_t)> handler)
{
    Q_D(MixedSslSocket);
    if (!d->initState && !d->useSSL)
        return d->wrappedSocket->sendAsync(buffer, std::move(handler));

    auto helper = static_cast< MixedSslAsyncBioHelper* >(d->asyncSslHelper.get());
    d->ioMode.store(SslSocket::ASYNC,std::memory_order_release);
    if (helper->is_initialized() && !helper->is_ssl())
        return d->wrappedSocket->sendAsync(buffer,std::move(handler));

    d->wrappedSocket->dispatch(
        [d, helper, &buffer, handler = std::move(handler)]() mutable
        {
            if (!d->initState)
                helper->set_ssl(d->useSSL);

            if (helper->is_initialized() && !helper->is_ssl())
                return d->wrappedSocket->sendAsync(buffer, std::move(handler));

            d->wrappedSocket->post(
                [helper, &buffer, handler = std::move(handler)]() mutable
                {
                    helper->asyncSend(buffer, std::move(handler));
                });
        });
}

SslServerSocket::SslServerSocket(
    AbstractStreamServerSocket* delegateSocket,
    bool allowNonSecureConnect)
:
    base_type([delegateSocket](){ return delegateSocket; }),
    m_allowNonSecureConnect(allowNonSecureConnect),
    m_delegateSocket(delegateSocket)
{
    initOpenSSLGlobalLock();
}

bool SslServerSocket::listen(int queueLen)
{
    return m_delegateSocket->listen(queueLen);
}

AbstractStreamSocket* SslServerSocket::accept()
{
    AbstractStreamSocket* acceptedSock = m_delegateSocket->accept();
    if (!acceptedSock)
        return nullptr;
    if (m_allowNonSecureConnect)
        return new MixedSslSocket(acceptedSock);
    else
        return new SslSocket(acceptedSock, true, true);
}

void SslServerSocket::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    return m_delegateSocket->pleaseStop(std::move(handler));
}

void SslServerSocket::acceptAsync(
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        AbstractStreamSocket*)> handler)
{
    using namespace std::placeholders;
    m_acceptHandler = std::move(handler);
    m_delegateSocket->acceptAsync(
        std::bind(&SslServerSocket::connectionAccepted, this, _1, _2));
}

void SslServerSocket::cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_delegateSocket->cancelIOAsync(std::move(handler));
}

void SslServerSocket::cancelIOSync()
{
    m_delegateSocket->cancelIOSync();
}

void SslServerSocket::connectionAccepted(
    SystemError::ErrorCode errorCode,
    AbstractStreamSocket* newSocket)
{
    if (newSocket)
    {
        if (m_allowNonSecureConnect)
            newSocket = new MixedSslSocket(newSocket);
        else
            newSocket = new SslSocket(newSocket, true, true);
    }

    auto handler = std::move(m_acceptHandler);
    handler(errorCode, newSocket);
}

} // namespace network
} // namespace nx

#endif // ENABLE_SSL
